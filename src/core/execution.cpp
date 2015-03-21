#include "medusa/execution.hpp"
#include "medusa/module.hpp"

#include "medusa/log.hpp"

MEDUSA_NAMESPACE_BEGIN

Execution::Execution(Document& rDoc, Architecture::SPType spArch, OperatingSystem::SPType spOs)
: m_rDoc(rDoc)
, m_spArch(spArch), m_spOs(spOs)
, m_pCpuCtxt(nullptr), m_pMemCtxt(nullptr)
, m_pCpuInfo(spArch->GetCpuInformation())
{
}

Execution::~Execution(void)
{
  delete m_pCpuCtxt;
  delete m_pMemCtxt;
}

bool Execution::Initialize(u8 Mode, std::vector<std::string> const& rArgs, std::vector<std::string> const& rEnv, std::string const& rCurWrkDir)
{
  delete m_pCpuCtxt;
  delete m_pMemCtxt;

  m_pCpuCtxt = m_spArch->MakeCpuContext();
  m_pMemCtxt = m_spArch->MakeMemoryContext();

  if (m_pCpuCtxt == nullptr || m_pMemCtxt == nullptr)
    return false;

  if (!m_pMemCtxt->MapDocument(m_rDoc, m_pCpuCtxt))
    return false;

  m_pCpuCtxt->SetMode(Mode);

  if (m_spOs == nullptr)
    return true;

  return m_spOs->InitializeContext(m_rDoc, *m_pCpuCtxt, *m_pMemCtxt, rArgs, rEnv, rCurWrkDir);
}

bool Execution::SetEmulator(std::string const& rEmulatorName)
{
  auto pGetEmulator = ModuleManager::Instance().GetEmulator(rEmulatorName);
  if (pGetEmulator == nullptr)
    return false;
  auto pEmul = pGetEmulator(m_spArch->GetCpuInformation(), m_pCpuCtxt, m_pMemCtxt);
  if (pEmul == nullptr)
    return false;
  m_spEmul = Emulator::SPType(pEmul);
  return true;
}

void Execution::Execute(Address const& rAddr)
{
  if (m_spEmul == nullptr)
    return;

  Address CurAddr = rAddr;

  u32 ProgPtrReg = m_pCpuInfo->GetRegisterByType(CpuInformation::ProgramPointerRegister, m_pCpuCtxt->GetMode());
  if (ProgPtrReg == CpuInformation::InvalidRegister)
    return;
  u32 ProgPtrRegSize = m_pCpuInfo->GetSizeOfRegisterInBit(ProgPtrReg);
  if (ProgPtrRegSize < 8)
    return;

  u64 CurInsn = rAddr.GetOffset();
  if (m_pCpuCtxt->WriteRegister(ProgPtrReg, &CurInsn, ProgPtrRegSize) == false)
    return;

  Address BlkAddr = CurAddr;
  while (true)
  {
    Expression::LSPType Sems;
    while (true)
    {
      auto spCurInsn = std::dynamic_pointer_cast<Instruction>(m_rDoc.GetCell(CurAddr));
      if (spCurInsn == nullptr)
      {
        Log::Write("exec").Level(LogInfo) << "not an instruction, try to disassemble it: " << CurAddr << LogEnd;
        TOffset CurOff;
        if (!m_rDoc.ConvertAddressToFileOffset(CurAddr, CurOff))
        {
          Log::Write("exec") << "instruction at " << CurAddr.ToString() << " is not contained in file" << LogEnd;
          Log::Write("exec") << "execution finished\n" << m_pCpuCtxt->ToString() << "\n" << m_pMemCtxt->ToString() << LogEnd;
          return;
        }
        auto spInsn = std::make_shared<Instruction>();
        if (!m_spArch->Disassemble(m_rDoc.GetBinaryStream(), CurOff, *spInsn, m_pCpuCtxt->GetMode()))
        {
          Log::Write("exec") << "unable to disassemble instruction at " << CurAddr.ToString() << LogEnd;
          Log::Write("exec") << "execution finished\n" << m_pCpuCtxt->ToString() << "\n" << m_pMemCtxt->ToString() << LogEnd;
          return;
        }

        if (!m_rDoc.SetCell(CurAddr, spInsn, true))
        {
          Log::Write("exec") << "unable to set a instruction at " << CurAddr.ToString() << LogEnd;
          Log::Write("exec") << "execution finished\n" << m_pCpuCtxt->ToString() << "\n" << m_pMemCtxt->ToString() << LogEnd;
          return;
        }
      }

      spCurInsn = std::dynamic_pointer_cast<Instruction>(m_rDoc.GetCell(CurAddr));
      if (spCurInsn == nullptr)
      {

        Log::Write("exec") << "unable to get instruction at " << CurAddr << LogEnd;
        Log::Write("exec") << "execution finished\n" << m_pCpuCtxt->ToString() << "\n" << m_pMemCtxt->ToString() << LogEnd;
        return;
      }

      Address PcAddr = m_spArch->CurrentAddress(CurAddr, *spCurInsn);

      Sems.push_back(Expr::MakeSys("dump_insn", CurAddr));

      // TODO: I'm not really satisfied with this method
      // it's not enough generic
      Sems.push_back(Expr::MakeAssign(
        Expr::MakeId(ProgPtrReg, m_pCpuInfo),
        Expr::MakeConst(PcAddr.GetOffsetSize(), PcAddr.GetOffset())));

      CurAddr.SetOffset(CurAddr.GetOffset() + spCurInsn->GetLength());

      auto const& rCurSem = spCurInsn->GetSemantic();
      if (rCurSem.empty())
      {
        Log::Write("exec").Level(LogWarning) << "no semantic available: " << spCurInsn->ToString() << LogEnd;
      }
      std::for_each(std::begin(rCurSem), std::end(rCurSem), [&](Expression::SPType spExpr)
      {
        Sems.push_back(spExpr->Clone());
      });

      Sems.push_back(Expr::MakeSys("check_exec_hook", Address()));

      if (spCurInsn->GetSubType() != Instruction::NoneType)
        break;
    };

    bool Res = m_spEmul->Execute(BlkAddr, Sems);

    if (Res == false)
    {
      Log::Write("exec") << "failed to execute block " << BlkAddr << LogEnd;
      Log::Write("exec") << "execution finished\n" << m_pCpuCtxt->ToString() << "\n" << m_pMemCtxt->ToString() << LogEnd;
      break;
    }

    u64 NextInsn = 0;
    if (!m_pCpuCtxt->ReadRegister(ProgPtrReg, &NextInsn, ProgPtrRegSize))
      break;
    CurAddr.SetOffset(NextInsn);
  }
}

bool Execution::HookInstruction(Emulator::HookCallback HkCb)
{
  if (m_spEmul == nullptr)
    return false;
  m_spEmul->AddHookOnInstruction(HkCb);
  return true;
}

bool Execution::HookFunction(std::string const& rFuncName, Emulator::HookCallback HkCb)
{
  if (m_spEmul == nullptr)
    return false;

  static u64 s_FakeAddr = 0xdead7700; // FIXME: this is a dirty hack
  auto const& rAddr   = m_rDoc.GetAddressFromLabelName(rFuncName);
  auto const& rLbl    = m_rDoc.GetLabelFromAddress(rAddr);

  if (!(rLbl.GetType() & (Label::Imported | Label::Function)))
    return false;

  auto const* pCpuInfo = m_spArch->GetCpuInformation();
  if (pCpuInfo == nullptr)
    return false;

  auto PcSize = pCpuInfo->GetSizeOfRegisterInBit(pCpuInfo->GetRegisterByType(CpuInformation::ProgramPointerRegister, m_rDoc.GetMode(rAddr))) / 8;
  if (PcSize == 0)
    return false;

  if (!m_spEmul->WriteMemory(rAddr, &s_FakeAddr, PcSize))
    return false;

  {
    std::lock_guard<std::mutex> Lock(m_HookMutex);
    m_HookName[s_FakeAddr] = rFuncName;
  }

  if (!m_spEmul->AddHook(s_FakeAddr, Emulator::HookOnExecute, HkCb))
    return false;

  s_FakeAddr += 4;

  return true;
}

std::string Execution::GetHookName(void) const
{
  u32 PrgReg = m_pCpuCtxt->GetCpuInformation().GetRegisterByType(CpuInformation::ProgramPointerRegister, m_pCpuCtxt->GetMode());
  if (PrgReg == 0)
    return "";
  auto PrgRegSize = m_pCpuCtxt->GetCpuInformation().GetSizeOfRegisterInBit(PrgReg);
  if (PrgRegSize == 0)
    return "";
  u64 PrgRegVal = 0;
  if (!m_pCpuCtxt->ReadRegister(PrgReg, &PrgRegVal, PrgRegSize))
    return "";

  std::lock_guard<std::mutex> Lock(m_HookMutex);
  auto itHookPair = m_HookName.find(PrgRegVal);
  if (itHookPair == std::end(m_HookName))
    return "";
  return itHookPair->second;
}

MEDUSA_NAMESPACE_END