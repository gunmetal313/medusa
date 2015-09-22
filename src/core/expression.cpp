#include "medusa/expression.hpp"
#include "medusa/extend.hpp"
#include <sstream>
#include <boost/format.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/reversed.hpp>

MEDUSA_NAMESPACE_USE

void Track::Context::TrackId(u32 Id, Address const& rCurAddr)
{
  m_TrackedId[Id] = rCurAddr;
}

bool Track::Context::GetTrackAddress(u32 RegId, Address& rTrackedAddress)
{
  auto itTrackedId = m_TrackedId.find(RegId);
  if (itTrackedId == std::end(m_TrackedId))
    return false;
  rTrackedAddress = itTrackedId->second;
  return true;
}

// system expression //////////////////////////////////////////////////////////

SystemExpression::SystemExpression(std::string const& rName, Address const& rAddr)
: m_Name(rName), m_Address(rAddr)
{
}

SystemExpression::~SystemExpression(void)
{
}

std::string SystemExpression::ToString(void) const
{
  return m_Address.ToString() + " " + m_Name;
}

Expression::SPType SystemExpression::Clone(void) const
{
  return std::make_shared<SystemExpression>(m_Name, m_Address);
}

Expression::SPType SystemExpression::Visit(ExpressionVisitor* pVisitor)
{
  return pVisitor->VisitSystem(std::static_pointer_cast<SystemExpression>(shared_from_this()));
}

Expression::CompareType SystemExpression::Compare(Expression::SPType spExpr) const
{
  auto spCmpExpr = expr_cast<SystemExpression>(spExpr);
  if (spCmpExpr == nullptr)
    return CmpDifferent;
  if (GetName() != spCmpExpr->GetName())
    return CmpSameExpression;
  if (GetAddress() != spCmpExpr->GetAddress())
    return CmpSameExpression;
  return CmpIdentical;
}

// bind expression ////////////////////////////////////////////////////////////

BindExpression::BindExpression(Expression::LSPType const& rExprs)
{
  for (auto spExpr : rExprs)
    m_Expressions.push_back(spExpr);
}

BindExpression::~BindExpression(void)
{
}

std::string BindExpression::ToString(void) const
{
  std::list<std::string> ExprsStrList;

  std::for_each(std::begin(m_Expressions), std::end(m_Expressions), [&](Expression::SPType spExpr)
  {
    if (spExpr != nullptr)
      ExprsStrList.push_back(spExpr->ToString());
  });

  return boost::algorithm::join(ExprsStrList, "; ");
}

Expression::SPType BindExpression::Clone(void) const
{
  Expression::LSPType ExprListCloned;
  std::for_each(std::begin(m_Expressions), std::end(m_Expressions), [&](Expression::SPType spExpr)
  {
    ExprListCloned.push_back(spExpr->Clone());
  });

  return std::make_shared<BindExpression>(ExprListCloned);
}

Expression::SPType BindExpression::Visit(ExpressionVisitor* pVisitor)
{
  return pVisitor->VisitBind(std::static_pointer_cast<BindExpression>(shared_from_this()));
}

bool BindExpression::UpdateChild(Expression::SPType spOldExpr, Expression::SPType spNewExpr)
{
  auto itExpr = std::find(std::begin(m_Expressions), std::end(m_Expressions), spOldExpr);
  if (itExpr != std::end(m_Expressions))
  {
    m_Expressions.erase(itExpr);
    // TODO: we may want to keep the order...
    m_Expressions.push_back(spNewExpr);
    return true;
  }

  for (auto spExpr : m_Expressions)
  {
    if (spExpr->UpdateChild(spOldExpr, spNewExpr))
      return true;
  }

  return false;
}

Expression::CompareType BindExpression::Compare(Expression::SPType spExpr) const
{
  auto spCmpExpr = expr_cast<BindExpression>(spExpr);
  if (spCmpExpr == nullptr)
    return CmpDifferent;

  auto const& rCmpBoundExprs = spCmpExpr->GetBoundExpressions();
  if (m_Expressions.size() != rCmpBoundExprs.size())
    return CmpSameExpression;

  auto itCmpExpr = std::begin(rCmpBoundExprs);
  for (auto const& rspExpr : m_Expressions)
  {
    if (rspExpr->Compare(*itCmpExpr) != CmpIdentical)
      return CmpSameExpression;
  }
  return CmpIdentical;
}

// condition expression ///////////////////////////////////////////////////////

ConditionExpression::ConditionExpression(Type CondType, Expression::SPType spRefExpr, Expression::SPType spTestExpr)
: m_Type(CondType), m_spRefExpr(spRefExpr), m_spTestExpr(spTestExpr)
{
}

ConditionExpression::~ConditionExpression(void)
{
}

std::string ConditionExpression::ToString(void) const
{
  static const char *s_StrCond[] = { "???", "==", "!=", "u>", "u>=", "u<", "u<=", "s>", "s>=", "s<", "s<=" };
  if (m_spRefExpr == nullptr || m_spTestExpr == nullptr || m_Type >= (sizeof(s_StrCond) / sizeof(*s_StrCond)))
    return "";

  return (boost::format("(%1% %2% %3%)") % m_spRefExpr->ToString() % s_StrCond[m_Type] % m_spTestExpr->ToString()).str();
}

Expression::CompareType ConditionExpression::Compare(Expression::SPType spExpr) const
{
  auto spCmpExpr = expr_cast<ConditionExpression>(spExpr);
  if (spCmpExpr == nullptr)
    return CmpDifferent;

  if (m_Type != spCmpExpr->GetType())
    return CmpSameExpression;
  if (m_spRefExpr->Compare(spCmpExpr->GetReferenceExpression()) != CmpIdentical)
    return CmpSameExpression;
  if (m_spTestExpr->Compare(spCmpExpr->GetTestExpression()) != CmpIdentical)
    return CmpSameExpression;
  return CmpIdentical;
}

TernaryConditionExpression::TernaryConditionExpression(Type CondType, Expression::SPType spRefExpr, Expression::SPType spTestExpr, Expression::SPType spTrueExpr, Expression::SPType spFalseExpr)
: ConditionExpression(CondType, spRefExpr, spTestExpr), m_spTrueExpr(spTrueExpr), m_spFalseExpr(spFalseExpr)
{
}

TernaryConditionExpression::~TernaryConditionExpression(void)
{
}

std::string TernaryConditionExpression::ToString(void) const
{
  std::string Result = ConditionExpression::ToString();
  Result += " ? (";
  Result += m_spTrueExpr->ToString();

  Result += ") : ";
  Result += m_spFalseExpr->ToString();

  Result += ")";

  return Result;
}

Expression::SPType TernaryConditionExpression::Clone(void) const
{
  return std::make_shared<TernaryConditionExpression>(
    m_Type,
    m_spRefExpr->Clone(),
    m_spTestExpr->Clone(),
    m_spTrueExpr->Clone(),
    m_spFalseExpr->Clone());
}

Expression::SPType TernaryConditionExpression::Visit(ExpressionVisitor* pVisitor)
{
  return pVisitor->VisitTernaryCondition(std::static_pointer_cast<TernaryConditionExpression>(shared_from_this()));
}

bool TernaryConditionExpression::UpdateChild(Expression::SPType spOldExpr, Expression::SPType spNewExpr)
{
  if (m_spRefExpr == spOldExpr)
  {
    m_spRefExpr = spNewExpr;
    return true;
  }

  if (m_spTestExpr == spOldExpr)
  {
    m_spTestExpr = spNewExpr;
    return true;
  }

  if (m_spTrueExpr == spOldExpr)
  {
    m_spTrueExpr = spNewExpr;
    return true;
  }

  if (m_spFalseExpr == spOldExpr)
  {
    m_spFalseExpr = spNewExpr;
    return true;
  }

  if (m_spRefExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  if (m_spTestExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  if (m_spTrueExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  if (m_spFalseExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  return false;
}

Expression::CompareType TernaryConditionExpression::Compare(Expression::SPType spExpr) const
{
  auto spCmpExpr = expr_cast<TernaryConditionExpression>(spExpr);
  if (spCmpExpr == nullptr)
    return CmpDifferent;
  if (ConditionExpression::Compare(spCmpExpr) != CmpIdentical)
    return CmpSameExpression;
  if (m_spTrueExpr->Compare(spCmpExpr->GetTrueExpression()) != CmpIdentical)
    return CmpSameExpression;
  if (m_spFalseExpr->Compare(spCmpExpr->GetFalseExpression()) != CmpIdentical)
    return CmpSameExpression;
  return CmpIdentical;
}

IfElseConditionExpression::IfElseConditionExpression(Type CondType, Expression::SPType spRefExpr, Expression::SPType spTestExpr, Expression::SPType spThenExpr, Expression::SPType spElseExpr)
: ConditionExpression(CondType, spRefExpr, spTestExpr), m_spThenExpr(spThenExpr), m_spElseExpr(spElseExpr)
{
}

IfElseConditionExpression::~IfElseConditionExpression(void)
{
}

std::string IfElseConditionExpression::ToString(void) const
{
  if (m_spElseExpr == nullptr)
    return (boost::format("if %1% { %2% }") % ConditionExpression::ToString() % m_spThenExpr->ToString()).str();
  return (boost::format("if %1% { %2% } else { %3% }") % ConditionExpression::ToString() % m_spThenExpr->ToString() % m_spElseExpr->ToString()).str();
}

Expression::SPType IfElseConditionExpression::Clone(void) const
{
  return Expr::MakeIfElseCond(
    m_Type,
    m_spRefExpr->Clone(), m_spTestExpr->Clone(),
    m_spThenExpr->Clone(), m_spElseExpr != nullptr ? m_spElseExpr->Clone() : nullptr);
}

Expression::SPType IfElseConditionExpression::Visit(ExpressionVisitor* pVisitor)
{
  return pVisitor->VisitIfElseCondition(std::static_pointer_cast<IfElseConditionExpression>(shared_from_this()));
}

bool IfElseConditionExpression::UpdateChild(Expression::SPType spOldExpr, Expression::SPType spNewExpr)
{
  if (m_spRefExpr == spOldExpr)
  {
    m_spRefExpr = spNewExpr;
    return true;
  }

  if (m_spTestExpr == spOldExpr)
  {
    m_spTestExpr = spNewExpr;
    return true;
  }

  if (m_spThenExpr == spOldExpr)
  {
    m_spThenExpr = spNewExpr;
    return true;
  }

  // NOTE: No need to check m_spElseExpr since spOldExpr cannot be nullptr

  if (m_spElseExpr == spOldExpr)
  {
    m_spElseExpr = spNewExpr;
    return true;
  }

  if (m_spRefExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  if (m_spTestExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  if (m_spThenExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  if (m_spElseExpr != nullptr && m_spElseExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  return false;
}

Expression::CompareType IfElseConditionExpression::Compare(Expression::SPType spExpr) const
{
  auto spCmpExpr = expr_cast<IfElseConditionExpression>(spExpr);
  if (spCmpExpr == nullptr)
    return CmpDifferent;
  if (ConditionExpression::Compare(spCmpExpr) != CmpIdentical)
    return CmpSameExpression;
  if (m_spThenExpr->Compare(spCmpExpr->GetThenExpression()) != CmpIdentical)
    return CmpSameExpression;
  if (m_spElseExpr->Compare(spCmpExpr->GetElseExpression()) != CmpIdentical)
    return CmpSameExpression;
  return CmpIdentical;
}

WhileConditionExpression::WhileConditionExpression(Type CondType, Expression::SPType spRefExpr, Expression::SPType spTestExpr, Expression::SPType spBodyExpr)
: ConditionExpression(CondType, spRefExpr, spTestExpr), m_spBodyExpr(spBodyExpr)
{
}

WhileConditionExpression::~WhileConditionExpression(void)
{
}

std::string WhileConditionExpression::ToString(void) const
{
  return (boost::format("while %1% { %2% }") % ConditionExpression::ToString() % m_spBodyExpr->ToString()).str();
}

Expression::SPType WhileConditionExpression::Clone(void) const
{
  return std::make_shared<WhileConditionExpression>(m_Type, m_spRefExpr->Clone(), m_spTestExpr->Clone(), m_spBodyExpr->Clone());
}

Expression::SPType WhileConditionExpression::Visit(ExpressionVisitor* pVisitor)
{
  return pVisitor->VisitWhileCondition(std::static_pointer_cast<WhileConditionExpression>(shared_from_this()));
}

bool WhileConditionExpression::UpdateChild(Expression::SPType spOldExpr, Expression::SPType spNewExpr)
{
  if (m_spRefExpr == spOldExpr)
  {
    m_spRefExpr = spNewExpr;
    return true;
  }

  if (m_spTestExpr == spOldExpr)
  {
    m_spTestExpr = spNewExpr;
    return true;
  }

  if (m_spBodyExpr == spOldExpr)
  {
    m_spBodyExpr = spNewExpr;
    return true;
  }

  if (m_spRefExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  if (m_spTestExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  if (m_spBodyExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  return false;
}

Expression::CompareType WhileConditionExpression::Compare(Expression::SPType spExpr) const
{
  auto spCmpExpr = expr_cast<WhileConditionExpression>(spExpr);
  if (spCmpExpr == nullptr)
    return CmpDifferent;
  if (ConditionExpression::Compare(spCmpExpr) != CmpIdentical)
    return CmpSameExpression;
  if (m_spBodyExpr->Compare(spCmpExpr->GetBodyExpression()) != CmpIdentical)
    return CmpSameExpression;
  return CmpIdentical;
}

// operation expression ///////////////////////////////////////////////////////

AssignmentExpression::AssignmentExpression(Expression::SPType spDstExpr, Expression::SPType spSrcExpr)
: m_spDstExpr(spDstExpr), m_spSrcExpr(spSrcExpr)
{
  assert(spDstExpr != nullptr && "Destination expression is null");
  assert(spSrcExpr != nullptr && "Source expression is null");
}

AssignmentExpression::~AssignmentExpression(void)
{
}

std::string AssignmentExpression::ToString(void) const
{
  return (boost::format("(%1% = %2%)") % m_spDstExpr->ToString() % m_spSrcExpr->ToString()).str();
}

Expression::SPType AssignmentExpression::Clone(void) const
{
  return std::make_shared<AssignmentExpression>(m_spDstExpr->Clone(), m_spSrcExpr->Clone());
}

Expression::SPType AssignmentExpression::Visit(ExpressionVisitor* pVisitor)
{
  return pVisitor->VisitAssignment(std::static_pointer_cast<AssignmentExpression>(shared_from_this()));
}

bool AssignmentExpression::UpdateChild(Expression::SPType spOldExpr, Expression::SPType spNewExpr)
{
  if (m_spDstExpr == spOldExpr)
  {
    m_spDstExpr = spNewExpr;
    return true;
  }

  if (m_spSrcExpr == spOldExpr)
  {
    m_spSrcExpr = spNewExpr;
    return true;
  }

  if (m_spDstExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  if (m_spSrcExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  return false;
}

Expression::CompareType AssignmentExpression::Compare(Expression::SPType spExpr) const
{
  auto spCmpExpr = expr_cast<AssignmentExpression>(spExpr);
  if (spCmpExpr == nullptr)
    return CmpDifferent;
  if (m_spDstExpr->Compare(spCmpExpr->GetDestinationExpression()) != CmpIdentical)
    return CmpSameExpression;
  if (m_spSrcExpr->Compare(spCmpExpr->GetSourceExpression()) != CmpIdentical)
    return CmpSameExpression;
  return CmpIdentical;
}

OperationExpression::OperationExpression(Type OpType)
: m_OpType(OpType)
{
}

OperationExpression::~OperationExpression(void)
{
}

std::string OperationExpression::ToString(void) const
{
  switch (m_OpType)
  {
    // Unknown / invalid
  case OpUnk: default: return "???";

    // unary
  case OpNot:  return "~";
  case OpNeg:  return "-";
  case OpSwap: return "⇄";
  case OpBsf:  return "bsf";
  case OpBsr:  return "bsr";

    // binary
  case OpXchg: return "↔";
  case OpAnd:  return "&";
  case OpOr:   return "|";
  case OpXor:  return "^";
  case OpLls:  return "<<";
  case OpLrs:  return ">>{u}";
  case OpArs:  return ">>{s}";
  case OpRol: return "rol";
  case OpRor: return "ror";
  case OpAdd:  return "+";
  case OpAddFloat:  return "+{f}";
  case OpSub:  return "-";
  case OpMul:  return "*";
  case OpSDiv: return "/{s}";
  case OpUDiv: return "/{u}";
  case OpSMod: return "%{s}";
  case OpUMod: return "%{u}";
  case OpSext: return "↗{s}";
  case OpZext: return "↗{z}";
  case OpInsertBits: return "<insert_bits>";
  case OpExtractBits: return "<extract_bits>";
  case OpBcast: return "<bcast>";
  }
}

u32 OperationExpression::GetBitSize(void) const
{
  return 0;
}

Expression::CompareType OperationExpression::Compare(Expression::SPType spExpr) const
{
  return CmpUnknown;
}

u8 OperationExpression::GetOppositeOperation(void) const
{
  // TODO:(KS)
  return OpUnk;
}

UnaryOperationExpression::UnaryOperationExpression(Type OpType, Expression::SPType spExpr)
  : OperationExpression(OpType), m_spExpr(spExpr)
{

}

UnaryOperationExpression::~UnaryOperationExpression(void)
{
}

std::string UnaryOperationExpression::ToString(void) const
{
  return (boost::format("%1%(%2%)") % OperationExpression::ToString() % m_spExpr->ToString()).str();
}

Expression::SPType UnaryOperationExpression::Clone(void) const
{
  return Expr::MakeUnOp(static_cast<OperationExpression::Type>(m_OpType),
    m_spExpr->Clone());
}

u32 UnaryOperationExpression::GetBitSize(void) const
{
  return m_spExpr->GetBitSize();
}

Expression::SPType UnaryOperationExpression::Visit(ExpressionVisitor* pVisitor)
{
  return pVisitor->VisitUnaryOperation(std::static_pointer_cast<UnaryOperationExpression>(shared_from_this()));
}

bool UnaryOperationExpression::UpdateChild(Expression::SPType spOldExpr, Expression::SPType spNewExpr)
{
  if (m_spExpr != spOldExpr)
    return false;

  m_spExpr = spNewExpr;
  return true;
}

Expression::CompareType UnaryOperationExpression::Compare(Expression::SPType spExpr) const
{
  auto spCmpExpr = expr_cast<UnaryOperationExpression>(spExpr);
  if (spCmpExpr == nullptr)
    return CmpDifferent;
  if (m_OpType != spCmpExpr->GetOperation())
    return CmpSameExpression;
  if (m_spExpr->Compare(spCmpExpr->GetExpression()) != CmpIdentical)
    return CmpSameExpression;
  return CmpIdentical;
}

BinaryOperationExpression::BinaryOperationExpression(Type OpType, Expression::SPType spLeftExpr, Expression::SPType spRightExpr)
  : OperationExpression(OpType), m_spLeftExpr(spLeftExpr), m_spRightExpr(spRightExpr)
{
  assert(spLeftExpr != nullptr);
  assert(spRightExpr != nullptr);
}

BinaryOperationExpression::~BinaryOperationExpression(void)
{
}

std::string BinaryOperationExpression::ToString(void) const
{
  std::string Res("(");
  Res += m_spLeftExpr->ToString();
  Res += " ";
  Res += OperationExpression::ToString();
  Res += " ";
  Res += m_spRightExpr->ToString();
  Res += ")";
  return Res;
  //return (boost::format("(%1% %2% %3%)") % m_spLeftExpr->ToString() % OperationExpression::ToString() % m_spRightExpr->ToString()).str();
}

Expression::SPType BinaryOperationExpression::Clone(void) const
{
  return Expr::MakeBinOp(static_cast<OperationExpression::Type>(m_OpType),
    m_spLeftExpr->Clone(), m_spRightExpr->Clone());
}

u32 BinaryOperationExpression::GetBitSize(void) const
{
  return std::max(m_spLeftExpr->GetBitSize(), m_spRightExpr->GetBitSize());
}

Expression::SPType BinaryOperationExpression::Visit(ExpressionVisitor* pVisitor)
{
  return pVisitor->VisitBinaryOperation(std::static_pointer_cast<BinaryOperationExpression>(shared_from_this()));
}

bool BinaryOperationExpression::UpdateChild(Expression::SPType spOldExpr, Expression::SPType spNewExpr)
{
  // LATER: What happens if left == right?
  if (m_spLeftExpr == spOldExpr)
  {
    m_spLeftExpr = spNewExpr;
    return true;
  }

  if (m_spRightExpr == spOldExpr)
  {
    m_spRightExpr = spNewExpr;
    return true;
  }

  if (m_spLeftExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  if (m_spRightExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  return false;
}

Expression::CompareType BinaryOperationExpression::Compare(Expression::SPType spExpr) const
{
  auto spCmpExpr = expr_cast<BinaryOperationExpression>(spExpr);
  if (spCmpExpr == nullptr)
    return CmpDifferent;
  if (m_OpType != spCmpExpr->GetOperation())
    return CmpSameExpression;
  if (m_spLeftExpr->Compare(spCmpExpr->GetLeftExpression()) != CmpIdentical)
    return CmpSameExpression;
  if (m_spRightExpr->Compare(spCmpExpr->GetRightExpression()) != CmpIdentical)
    return CmpSameExpression;
  return CmpIdentical;
}

void BinaryOperationExpression::SwapLeftExpressions(BinaryOperationExpression::SPType spOpExpr)
{
  m_spLeftExpr.swap(spOpExpr->m_spLeftExpr);
  if (m_OpType == OpSub && spOpExpr->m_OpType == OpSub) // TODO: handle operator precedence
    spOpExpr->m_OpType = OpAdd;
  assert(this != m_spLeftExpr.get());
}

// constant expression ////////////////////////////////////////////////////////

BitVectorExpression::BitVectorExpression(u16 BitSize, ap_int Value)
: m_Value(BitSize, Value)
{
}

BitVectorExpression::BitVectorExpression(BitVector const& rValue)
: m_Value(rValue)
{
}

std::string BitVectorExpression::ToString(void) const
{
  std::string Res("int");
  Res += std::to_string(m_Value.GetBitSize());
  Res += "(";
  Res += m_Value.ToString();
  Res += ")";
  return Res;
  //return (boost::format("int%d(%x)") % m_Value.GetBitSize() % m_Value.ToString()).str();
}

Expression::SPType BitVectorExpression::Clone(void) const
{
  return std::make_shared<BitVectorExpression>(m_Value);
}

Expression::SPType BitVectorExpression::Visit(ExpressionVisitor* pVisitor)
{
  return pVisitor->VisitBitVector(std::static_pointer_cast<BitVectorExpression>(shared_from_this()));
}

Expression::CompareType BitVectorExpression::Compare(Expression::SPType spExpr) const
{
  auto spCmpExpr = expr_cast<BitVectorExpression>(spExpr);
  if (spCmpExpr == nullptr)
    return CmpDifferent;
  if (m_Value.GetUnsignedValue() != spCmpExpr->GetInt().GetUnsignedValue())
    return CmpSameExpression;
  return CmpIdentical;
}

bool BitVectorExpression::Read(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, DataContainerType& rData) const
{
  if (rData.size() != 1)
    return false;

  rData.front() = m_Value;
  return true;
}

bool BitVectorExpression::Write(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, DataContainerType& rData)
{
  return false;
}

bool BitVectorExpression::GetAddress(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, Address& rAddress) const
{
  return false;
}

// IntegerExpression::IntegerExpression(u16 BitSize, ap_int Value)
// : m_Value(BitSize, Value)
// {
// }

// IntegerExpression::IntegerExpression(BitVector const& rValue)
// : m_Value(rValue)
// {
// }

// std::string IntegerExpression::ToString(void) const
// {
//   std::string Res("int");
//   Res += std::to_string(m_Value.GetBitSize());
//   Res += "(";
//   Res += m_Value.ToString();
//   Res += ")";
//   return Res;
//   //return (boost::format("int%d(%x)") % m_Value.GetBitSize() % m_Value.ToString()).str();
// }

// Expression::SPType IntegerExpression::Clone(void) const
// {
//   return std::make_shared<IntegerExpression>(m_Value);
// }

// Expression::SPType IntegerExpression::Visit(ExpressionVisitor* pVisitor)
// {
//   return pVisitor->VisitBitVector(std::static_pointer_cast<IntegerExpression>(shared_from_this()));
// }

// Expression::CompareType IntegerExpression::Compare(Expression::SPType spExpr) const
// {
//   auto spCmpExpr = expr_cast<IntegerExpression>(spExpr);
//   if (spCmpExpr == nullptr)
//     return CmpDifferent;
//   if (m_Value.GetUnsignedValue() != spCmpExpr->GetInt().GetUnsignedValue())
//     return CmpSameExpression;
//   return CmpIdentical;
// }

// bool IntegerExpression::Read(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, DataContainerType& rData) const
// {
//   if (rData.size() != 1)
//     return false;

//   rData.front() = m_Value;
//   return true;
// }

// bool IntegerExpression::Write(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, DataContainerType& rData)
// {
//   return false;
// }

// bool IntegerExpression::GetAddress(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, Address& rAddress) const
// {
//   return false;
// }

// FloatingPointExpression::FloatingPointExpression(float const Value)
// {
//   m_Value.sgl = Value;
//   m_Precision = FloatingPointExpression::Single;
// }

// FloatingPointExpression::FloatingPointExpression(double const Value)
// {
//   m_Value.dbl = Value;
//   m_Precision = FloatingPointExpression::Double;
// }

// FloatingPointExpression::FloatingPointExpression(FloatingType const& rValue)
// : m_Value(rValue)
// {
// }

// std::string FloatingPointExpression::ToString(void) const
// {
//   std::string Res("float");
//   Res += std::to_string(GetBitSize());
//   Res += "(";

//   std::ostringstream Tmp;

//   switch (m_Precision)
//   {
//   case FloatingPointExpression::Single: Tmp << m_Value.sgl; break;
//   case FloatingPointExpression::Double: Tmp << m_Value.dbl; break;
//   default: return "<invalid variable>";
//   }

//   Res += Tmp.str() + ")";
//   return Res;
// }

// u32 FloatingPointExpression::GetBitSize(void) const
// {
//   switch (m_Precision)
//   {
//   case FloatingPointExpression::Single: return 32; break;
//   case FloatingPointExpression::Double: return 64; break;
//   default: return 0;
//   }
// }

// Expression::CompareType FloatingPointExpression::Compare(Expression::SPType spExpr) const
// {
//   auto spCmpExpr = expr_cast<FloatingPointExpression>(spExpr);
//   if (spCmpExpr == nullptr)
//     return CmpDifferent;

//   switch (m_Precision)
//   {
//   case FloatingPointExpression::Single:
//     if (m_Value.sgl != spCmpExpr->GetSingle()) return CmpSameExpression;
//   break;
//   case FloatingPointExpression::Double:
//     if (m_Value.dbl != spCmpExpr->GetDouble()) return CmpSameExpression;
//   break;
//   default: return CmpDifferent;
//   }

//   return CmpIdentical;
// }

// Expression::SPType FloatingPointExpression::Clone(void) const
// {
//   return std::make_shared<FloatingPointExpression>(m_Value);
// }

// Expression::SPType FloatingPointExpression::Visit(ExpressionVisitor* pVisitor)
// {
//   return pVisitor->VisitFloat(std::static_pointer_cast<FloatingPointExpression>(shared_from_this()));
// }

// bool FloatingPointExpression::Read(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, DataContainerType& rData) const
// {
//   if (rData.size() != 1)
//   return false;

//   switch (m_Precision)
//   {
//   case FloatingPointExpression::Single:
//     rData.front() = BitVector(*reinterpret_cast<u32 const*>(&m_Value.sgl));
//   break;
//   case FloatingPointExpression::Double:
//     rData.front() = BitVector(*reinterpret_cast<u64 const*>(&m_Value.dbl));
//   break;
//   default: return false;
//   }

//   return true;
// }

// bool FloatingPointExpression::Write(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, DataContainerType& rData)
// {
//   return false;
// }

// bool FloatingPointExpression::GetAddress(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, Address& rAddress) const
// {
//   return false;
// }

// identifier expression //////////////////////////////////////////////////////

IdentifierExpression::IdentifierExpression(u32 Id, CpuInformation const* pCpuInfo)
: m_Id(Id), m_pCpuInfo(pCpuInfo) {}


std::string IdentifierExpression::ToString(void) const
{
  auto pIdName = m_pCpuInfo->ConvertIdentifierToName(m_Id);

  if (pIdName == 0) return "";

  return (boost::format("Id%d(%s)") % m_pCpuInfo->GetSizeOfRegisterInBit(m_Id) % pIdName).str();
}

Expression::SPType IdentifierExpression::Clone(void) const
{
  return std::make_shared<IdentifierExpression>(m_Id, m_pCpuInfo);
}

u32 IdentifierExpression::GetBitSize(void) const
{
  return m_pCpuInfo->GetSizeOfRegisterInBit(m_Id);
}

Expression::SPType IdentifierExpression::Visit(ExpressionVisitor* pVisitor)
{
  return pVisitor->VisitIdentifier(std::static_pointer_cast<IdentifierExpression>(shared_from_this()));
}

Expression::CompareType IdentifierExpression::Compare(Expression::SPType spExpr) const
{
  auto spCmpExpr = expr_cast<IdentifierExpression>(spExpr);
  if (spCmpExpr == nullptr)
    return CmpDifferent;
  if (m_Id != spCmpExpr->GetId())
    return CmpSameExpression;
  if (m_pCpuInfo != spCmpExpr->GetCpuInformation())
    return CmpSameExpression;
  return CmpIdentical;
}

bool IdentifierExpression::Read(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, DataContainerType& rData) const
{
  if (rData.size() != 1)
    return false;

  u32 RegSize = m_pCpuInfo->GetSizeOfRegisterInBit(m_Id);

  u64 Value = 0;
  if (!pCpuCtxt->ReadRegister(m_Id, &Value, RegSize))
    return false;

  rData.front() = BitVector(RegSize, ap_int(Value));
  return true;
}

bool IdentifierExpression::Write(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, DataContainerType& rData)
{
  auto DataValue = rData.front();

  u32 RegSize = m_pCpuInfo->GetSizeOfRegisterInBit(m_Id);
  if (RegSize != DataValue.GetBitSize())
    Log::Write("core").Level(LogDebug) << "mismatch type when writing into an identifier: " << ToString() << " id size: " << RegSize << " write size: " << DataValue.GetBitSize() << LogEnd;

  u64 RegVal = DataValue.ConvertTo<u64>();
  if (!pCpuCtxt->WriteRegister(m_Id, &RegVal, RegSize))
    return false;

  rData.pop_front();
  return true;
}

bool IdentifierExpression::GetAddress(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, Address& rAddress) const
{
  return false;
}

VectorIdentifierExpression::VectorIdentifierExpression(std::vector<u32> const& rVecId, CpuInformation const* pCpuInfo)
  : m_VecId(rVecId), m_pCpuInfo(pCpuInfo)
{
}

std::string VectorIdentifierExpression::ToString(void) const
{
  std::vector<std::string> VecStr;
  VecStr.reserve(m_VecId.size());
  for (auto Id : m_VecId)
  {
    auto pCurIdStr = m_pCpuInfo->ConvertIdentifierToName(Id);
    if (pCurIdStr == nullptr)
      return "";
    VecStr.push_back(pCurIdStr);
  }
  return "{ " + boost::algorithm::join(VecStr, ", ") + " }";
}

Expression::SPType VectorIdentifierExpression::Clone(void) const
{
  return std::make_shared<VectorIdentifierExpression>(m_VecId, m_pCpuInfo);
}

u32 VectorIdentifierExpression::GetBitSize(void) const
{
  u32 SizeInBit = 0;
  for (auto Id : m_VecId)
  {
    SizeInBit += m_pCpuInfo->GetSizeOfRegisterInBit(Id);
  }
  return SizeInBit;
}

Expression::SPType VectorIdentifierExpression::Visit(ExpressionVisitor* pVisitor)
{
  return pVisitor->VisitVectorIdentifier(std::static_pointer_cast<VectorIdentifierExpression>(shared_from_this()));
}

Expression::CompareType VectorIdentifierExpression::Compare(Expression::SPType spExpr) const
{
  auto spCmpExpr = expr_cast<VectorIdentifierExpression>(spExpr);
  if (spCmpExpr == nullptr)
    return CmpDifferent;
  if (m_pCpuInfo != spCmpExpr->GetCpuInformation())
    return CmpSameExpression;
  auto CmpVecId = spCmpExpr->GetVector();
  if (m_VecId.size() != CmpVecId.size())
    return CmpSameExpression;
  if (!std::equal(std::begin(m_VecId), std::end(m_VecId), std::begin(CmpVecId)))
    return CmpSameExpression;
  return CmpIdentical;
}

void VectorIdentifierExpression::Prepare(DataContainerType& rData) const
{
  rData.resize(2);
}

bool VectorIdentifierExpression::Read(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, DataContainerType& rData) const
{
  // FIXME: we ignore the initial size of rData here
  rData.clear();

  for (auto Id : m_VecId)
  {
    u32 RegSize = m_pCpuInfo->GetSizeOfRegisterInBit(Id);
    u64 RegValue = 0;
    if (!pCpuCtxt->ReadRegister(Id, &RegValue, RegSize))
      return false;
    rData.push_front(BitVector(RegSize, RegValue));
  }
  return true;
}

bool VectorIdentifierExpression::Write(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, DataContainerType& rData)
{
  for (auto Id : boost::adaptors::reverse(m_VecId))
  {
    if (rData.empty())
      return false;
    u32 RegSize = m_pCpuInfo->GetSizeOfRegisterInBit(Id);
    auto DataValue = rData.front().ConvertTo<u64>();
    if (!pCpuCtxt->WriteRegister(Id, &DataValue, RegSize))
      return false;
    rData.pop_front();
  }
  return true;
}

bool VectorIdentifierExpression::GetAddress(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, Address& rAddress) const
{
  return false;
}

TrackExpression::TrackExpression(Expression::SPType spTrkExpr, Address const& rCurAddr, u8 Pos)
: m_spTrkExpr(spTrkExpr), m_CurAddr(rCurAddr), m_Pos(Pos) {}


TrackExpression::~TrackExpression(void)
{
}

std::string TrackExpression::ToString(void) const
{
  return (boost::format("Trk(%s, %d, %s)") % m_CurAddr.ToString() % static_cast<unsigned int>(m_Pos) % m_spTrkExpr->ToString()).str();
}

Expression::SPType TrackExpression::Clone(void) const
{
  return std::make_shared<TrackExpression>(m_spTrkExpr, m_CurAddr, m_Pos);
}

u32 TrackExpression::GetBitSize(void) const
{
  return m_spTrkExpr->GetBitSize();
}

Expression::SPType TrackExpression::Visit(ExpressionVisitor* pVisitor)
{
  return pVisitor->VisitTrack(std::static_pointer_cast<TrackExpression>(shared_from_this()));
}

Expression::CompareType TrackExpression::Compare(Expression::SPType spExpr) const
{
  auto spCmpExpr = expr_cast<TrackExpression>(spExpr);
  if (spCmpExpr == nullptr)
    return CmpDifferent;
  if (m_CurAddr != spCmpExpr->GetTrackAddress())
    return CmpSameExpression;
  if (m_Pos != spCmpExpr->GetTrackPosition())
    return CmpSameExpression;
  return CmpIdentical;
}

// variable ///////////////////////////////////////////////////////////////////

VariableExpression::VariableExpression(std::string const& rVarName, ActionType VarAct, u32 BitSize)
  : m_Name(rVarName), m_Action(VarAct), m_BitSize(BitSize)
{
  if (BitSize != 0 && VarAct != Alloc)
    Log::Write("core").Level(LogWarning) << "variable expression doesn't require bit size if action is different from ``alloc''" << LogEnd;
  if (BitSize == 0 && VarAct == Alloc)
    Log::Write("core").Level(LogWarning) << "try to allocate a 0-bit variable" << LogEnd;
}

VariableExpression::~VariableExpression(void)
{
}

std::string VariableExpression::ToString(void) const
{
  std::string ActStr = "";

  switch (m_Action)
  {
  case Alloc: ActStr = "alloc"; break;
  case Free: ActStr = "free"; break;
  case Use: ActStr = "use"; break;
  default: return "<invalid variable>";
  }

  return (boost::format("Var%d[%s] %s") % m_BitSize % ActStr % m_Name).str();
}

Expression::SPType VariableExpression::Clone(void) const
{
  return Expr::MakeVar(m_Name, m_Action, m_BitSize);
}

u32 VariableExpression::GetBitSize(void) const
{
  return m_BitSize;
}

Expression::SPType VariableExpression::Visit(ExpressionVisitor* pVisitor)
{
  return pVisitor->VisitVariable(std::static_pointer_cast<VariableExpression>(shared_from_this()));
}

Expression::CompareType VariableExpression::Compare(Expression::SPType spExpr) const
{
  auto spCmpExpr = expr_cast<VariableExpression>(spExpr);
  if (spCmpExpr == nullptr)
    return CmpDifferent;
  if (m_Action != spCmpExpr->GetAction())
    return CmpSameExpression;
  if (m_Name != spCmpExpr->GetName())
    return CmpSameExpression;
  return CmpIdentical;
}

// memory expression //////////////////////////////////////////////////////////

MemoryExpression::MemoryExpression(u32 AccessSize, Expression::SPType spExprBase, Expression::SPType spExprOffset, bool Dereference)
: m_AccessSizeInBit(AccessSize), m_spBaseExpr(spExprBase), m_spOffExpr(spExprOffset), m_Dereference(Dereference)
{
  assert(spExprOffset != nullptr);
}

MemoryExpression::~MemoryExpression(void)
{
}

std::string MemoryExpression::ToString(void) const
{
  auto const pMemType = m_Dereference ? "Mem" : "Addr";
  if (m_spBaseExpr == nullptr)
    return (boost::format("%s%d(%s)") % pMemType % m_AccessSizeInBit % m_spOffExpr->ToString()).str();

  return (boost::format("%s%d(%s:%s)") % pMemType % m_AccessSizeInBit % m_spBaseExpr->ToString() % m_spOffExpr->ToString()).str();
}

Expression::SPType MemoryExpression::Clone(void) const
{
  if (m_spBaseExpr == nullptr)
    return std::make_shared<MemoryExpression>(m_AccessSizeInBit, nullptr, m_spOffExpr->Clone(), m_Dereference);

  return std::make_shared<MemoryExpression>(m_AccessSizeInBit, m_spBaseExpr->Clone(), m_spOffExpr->Clone(), m_Dereference);
}

u32 MemoryExpression::GetBitSize(void) const
{
  return m_AccessSizeInBit;
}

Expression::SPType MemoryExpression::Visit(ExpressionVisitor* pVisitor)
{
  return pVisitor->VisitMemory(std::static_pointer_cast<MemoryExpression>(shared_from_this()));
}

bool MemoryExpression::Read(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, DataContainerType& rData) const
{
  Address DstAddr;
  if (GetAddress(pCpuCtxt, pMemCtxt, DstAddr) == false)
    return false;

  u64 LinAddr = 0;
  if (pCpuCtxt->Translate(DstAddr, LinAddr) == false)
    LinAddr = DstAddr.GetOffset();

  if (m_Dereference == true)
  {
    for (auto& rDataValue : rData)
    {
      u64 MemVal = 0;
      if (!pMemCtxt->ReadMemory(LinAddr, &MemVal, m_AccessSizeInBit / 8))
        return false;
      rDataValue = BitVector(m_AccessSizeInBit, MemVal);
    }
  }
  else
  {
    if (rData.size() != 1)
      return false;
    rData.front() = BitVector(m_AccessSizeInBit, LinAddr);
  }

  return true;
}

bool MemoryExpression::Write(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, DataContainerType& rData)
{
  // When we dereference this expression, we actually have to write stuff in memory
  if (m_Dereference)
  {
    Address DstAddr;
    if (GetAddress(pCpuCtxt, pMemCtxt, DstAddr) == false)
      return false;
    u64 LinAddr = 0;
    if (pCpuCtxt->Translate(DstAddr, LinAddr) == false)
      LinAddr = DstAddr.GetOffset();

    for (auto const& rDataValue : rData)
    {
      auto Val = rDataValue.ConvertTo<u64>();
      if (!pMemCtxt->WriteMemory(LinAddr, &Val, rDataValue.GetBitSize() / 8))
        return false;
      LinAddr += rDataValue.GetBitSize() / 8;
    }
  }
  // but if it's just an addressing operation, we have to make sure the address is moved
  // TODO: however, this kind of operation could modify both base and offset.
  // At this time we only modify the offset value if it's a register (otherwise it has to fail)
  else
  {
    // We only handle one data
    if (rData.size() != 1)
      return false;

    auto spRegOff = expr_cast<IdentifierExpression>(m_spOffExpr);
    if (spRegOff == nullptr)
      return false;

    //auto DataSize = std::get<0>(rData.front());
    auto DataVal = rData.front().ConvertTo<u64>();
    u64 DataToWrite = DataVal; // TODO: is it mandatory?
    if (!pCpuCtxt->WriteRegister(spRegOff->GetId(), &DataToWrite, spRegOff->GetBitSize()))
      return false;
  }
  return true;
}

bool MemoryExpression::GetAddress(CpuContext *pCpuCtxt, MemoryContext* pMemCtxt, Address& rAddress) const
{
  Expression::DataContainerType BaseData(1), OffsetData(1);

  if (m_spBaseExpr != nullptr)
    if (m_spBaseExpr->Read(pCpuCtxt, pMemCtxt, BaseData) == false)
      return false;
  if (m_spOffExpr->Read(pCpuCtxt, pMemCtxt, OffsetData) == false)
    return false;

  TBase Base = BaseData.size() != 1 ? 0x0 : BaseData.front().ConvertTo<TBase>();
  if (OffsetData.size() != 1)
    return false;
  TOffset OffsetValue = OffsetData.front().ConvertTo<TOffset>();
  rAddress = Address(Base, OffsetValue);
  return true;
}

Expression::SPType MemoryExpression::ToAddress(void) const
{
  return Expr::MakeMem(m_AccessSizeInBit,
    m_spBaseExpr,
    m_spOffExpr,
    false);
}

bool MemoryExpression::UpdateChild(Expression::SPType spOldExpr, Expression::SPType spNewExpr)
{
  if (m_spBaseExpr == spOldExpr)
  {
    m_spBaseExpr = spNewExpr;
    return true;
  }

  if (m_spOffExpr == spOldExpr)
  {
    m_spOffExpr = spNewExpr;
    return true;
  }

  if (m_spBaseExpr != nullptr && m_spBaseExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  if (m_spOffExpr->UpdateChild(spOldExpr, spNewExpr))
    return true;

  return false;
}

Expression::CompareType MemoryExpression::Compare(Expression::SPType spExpr) const
{
  auto spCmpExpr = expr_cast<MemoryExpression>(spExpr);
  if (spCmpExpr == nullptr)
    return CmpDifferent;
  if (m_AccessSizeInBit != spCmpExpr->GetAccessSizeInBit())
    return CmpSameExpression;
  auto spCmpBaseExpr = spCmpExpr->GetBaseExpression();
  if (m_spBaseExpr == nullptr && spCmpBaseExpr != nullptr)
    return CmpSameExpression;
  if (m_spBaseExpr != nullptr && m_spBaseExpr->Compare(spCmpBaseExpr) != CmpIdentical)
    return CmpSameExpression;
  if (m_spOffExpr->Compare(spCmpExpr->GetOffsetExpression()) != CmpIdentical)
    return CmpSameExpression;
  if (m_Dereference != spCmpExpr->IsDereferencable())
    return CmpSameExpression;
  return CmpIdentical;
}

// symbolic expression ////////////////////////////////////////////////////////

SymbolicExpression::SymbolicExpression(SymbolicExpression::Type SymType, std::string const& rValue, Address const& rAddr, Expression::SPType spExpr)
: m_Type(SymType), m_Value(rValue), m_Address(rAddr), m_spExpr(spExpr) {}

std::string SymbolicExpression::ToString(void) const
{
  static char const* TypeToStr[] = { "unknown", "retval", "parm", "undef" };
  if (m_Type > Undefined)
    return "<invalid symbolic expression>";
  if (m_spExpr == nullptr)
    return (boost::format("Sym(%s, \"%s\", %s)") % TypeToStr[m_Type] % m_Value % m_Address.ToString()).str();
  return (boost::format("Sym(%s, \"%s\", %s, %s)") % TypeToStr[m_Type] % m_Value % m_Address.ToString() % m_spExpr->ToString()).str();
}

Expression::SPType SymbolicExpression::Clone(void) const
{
  return std::make_shared<SymbolicExpression>(m_Type, m_Value, m_Address, m_spExpr);
}

u32 SymbolicExpression::GetBitSize(void) const
{
  return 0;
}

Expression::SPType SymbolicExpression::Visit(ExpressionVisitor* pVisitor)
{
  return pVisitor->VisitSymbolic(std::static_pointer_cast<SymbolicExpression>(shared_from_this()));
}

bool SymbolicExpression::UpdateChild(Expression::SPType spOldExpr, Expression::SPType spNewExpr)
{
  if (m_spExpr->Compare(spOldExpr) == Expression::CmpIdentical)
  {
    m_spExpr = spNewExpr;
    return true;
  }

  return m_spExpr->UpdateChild(spOldExpr, spNewExpr);
}

Expression::CompareType SymbolicExpression::Compare(Expression::SPType spExpr) const
{
  auto spCmpExpr = expr_cast<SymbolicExpression>(spExpr);
  if (spCmpExpr == nullptr)
    return CmpDifferent;
  if (m_Address != spCmpExpr->GetAddress())
    return CmpSameExpression;
  if (m_Value != spCmpExpr->GetValue())
    return CmpSameExpression;
  if (m_Type != spCmpExpr->GetType())
    return CmpSameExpression;
  if (m_spExpr->Compare(spCmpExpr->GetExpression()) != CmpIdentical)
    return CmpSameExpression;
  return CmpIdentical;
}

// helper /////////////////////////////////////////////////////////////////////

Expression::SPType Expr::MakeBitVector(BitVector const& rValue)
{
  return std::make_shared<BitVectorExpression>(rValue);
}

Expression::SPType Expr::MakeBitVector(u16 BitSize, ap_int Value)
{
  return std::make_shared<BitVectorExpression>(BitSize, Value);
}

Expression::SPType Expr::MakeBoolean(bool Value)
{
  return std::make_shared<BitVectorExpression>(1, Value ? 1 : 0);
}

Expression::SPType Expr::MakeId(u32 Id, CpuInformation const* pCpuInfo)
{
  return std::make_shared<IdentifierExpression>(Id, pCpuInfo);
}

Expression::SPType Expr::MakeVecId(std::vector<u32> const& rVecId, CpuInformation const* pCpuInfo)
{
  return std::make_shared<VectorIdentifierExpression>(rVecId, pCpuInfo);
}

Expression::SPType Expr::MakeTrack(Expression::SPType spTrkExpr, Address const& rCurAddr, u8 Pos)
{
  return std::make_shared<TrackExpression>(spTrkExpr, rCurAddr, Pos);
}

Expression::SPType Expr::MakeMem(u32 AccessSize, Expression::SPType spExprBase, Expression::SPType spExprOffset, bool Dereference)
{
  return std::make_shared<MemoryExpression>(AccessSize, spExprBase, spExprOffset, Dereference);
}

Expression::SPType Expr::MakeVar(std::string const& rName, VariableExpression::ActionType Act, u16 BitSize)
{
  return std::make_shared<VariableExpression>(rName, Act, BitSize);
}

Expression::SPType Expr::MakeTernaryCond(ConditionExpression::Type CondType, Expression::SPType spRefExpr, Expression::SPType spTestExpr, Expression::SPType spTrueExpr, Expression::SPType spFalseExpr)
{
  return std::make_shared<TernaryConditionExpression>(CondType, spRefExpr, spTestExpr, spTrueExpr, spFalseExpr);
}

Expression::SPType Expr::MakeIfElseCond(ConditionExpression::Type CondType, Expression::SPType spRefExpr, Expression::SPType spTestExpr, Expression::SPType spThenExpr, Expression::SPType spElseExpr)
{
  return std::make_shared<IfElseConditionExpression>(CondType, spRefExpr, spTestExpr, spThenExpr, spElseExpr);
}

Expression::SPType Expr::MakeWhileCond(ConditionExpression::Type CondType, Expression::SPType spRefExpr, Expression::SPType spTestExpr, Expression::SPType spBodyExpr)
{
  return std::make_shared<WhileConditionExpression>(CondType, spRefExpr, spTestExpr, spBodyExpr);
}

Expression::SPType Expr::MakeAssign(Expression::SPType spDstExpr, Expression::SPType spSrcExpr)
{
  return std::make_shared<AssignmentExpression>(spDstExpr, spSrcExpr);
}

Expression::SPType Expr::MakeUnOp(OperationExpression::Type OpType, Expression::SPType spExpr)
{
  return std::make_shared<UnaryOperationExpression>(OpType, spExpr);
}

Expression::SPType Expr::MakeBinOp(OperationExpression::Type OpType, Expression::SPType spLeftExpr, Expression::SPType spRightExpr)
{
  return std::make_shared<BinaryOperationExpression>(OpType, spLeftExpr, spRightExpr);
}

Expression::SPType Expr::MakeBind(Expression::LSPType const& rExprs)
{
  return std::make_shared<BindExpression>(rExprs);
}

Expression::SPType Expr::MakeSym(SymbolicExpression::Type SymType, std::string const& rValue, Address const& rAddr, Expression::SPType spExpr)
{
  return std::make_shared<SymbolicExpression>(SymType, rValue, rAddr, spExpr);
}

Expression::SPType Expr::MakeSys(std::string const& rName, Address const& rAddr)
{
  return std::make_shared<SystemExpression>(rName, rAddr);
}

bool Expr::TestKind(Expression::Kind Kind, Expression::SPType spExpr)
{
  return spExpr->IsKindOf(Kind);
}