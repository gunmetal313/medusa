#ifndef QMEDUSA_CFG_SCENE_HPP
#define QMEDUSA_CFG_SCENE_HPP

#include <QtCore>
#include <QtGui>
#include <QGraphicsScene>

#include <medusa/medusa.hpp>
#include <medusa/function.hpp>

class ControlFlowGraphScene : public QGraphicsScene
{
  Q_OBJECT
public:
  explicit ControlFlowGraphScene(QObject* pParent, medusa::Medusa& rCore, medusa::Address const& rCfgAddr);

private:
  bool _Update(void);

  medusa::Medusa& m_rCore;
  medusa::Address m_CfgAddr;
};

#endif // QMEDUSA_CFG_SCENE_HPP