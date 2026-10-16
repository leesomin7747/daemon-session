#include "SceneModel.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
SceneModel::SceneModel(QObject* p):QObject(p) { qRegisterMetaType<Action>(); }
QVariantMap SceneModel::peer(const QString& id) const {return peerStates().value(id).toMap();}
void SceneModel::setTransportConnected(bool connected) {if(m_transportConnected==connected) return; m_transportConnected=connected; emit peersChanged();}
bool SceneModel::showLocalOffline() const {return m_showLocalOffline;}
void SceneModel::setShowLocalOffline(bool show) {if(m_showLocalOffline==show) return; m_showLocalOffline=show; emit showLocalOfflineChanged(); emit peersChanged();}
void SceneModel::resetSession() {emit sceneReset(); m_state={}; m_lastEvent.clear(); emit peersChanged(); emit lastEventChanged();}
QVariantMap SceneModel::peerStates() const
{
    QVariantMap result; int index=0;
    for (const auto& p:m_state.peers) {
        QVariantList inbox;
        for (const auto& item:p.inbox) inbox.append(QVariantMap{{"eventId",item.eventId},
            {"kind",actionName(item.kind)},{"text",item.text}});
        result.insert(p.id,QVariantMap{{"id",p.id},{"reachable",m_transportConnected && p.reachable},{"busy",p.busy},
            {"wip",p.wip},{"x",p.x},{"pinned",p.pinned},{"inbox",inbox},{"tint",index++==0?"#bad8ba":"#e2b5a2"}});
    }
    if(result.isEmpty() && !m_localPeerId.isEmpty()) result.insert(m_localPeerId,QVariantMap{{"id",m_localPeerId},{"reachable",false},{"busy",false},{"wip",""},{"x",0.25},{"pinned",true},{"inbox",QVariantList{}},{"tint","#bad8ba"}});
    return result;
}
QVariantList SceneModel::peers() const {
    QVariantList result;
    for(const auto& value:peerStates()) {
        const auto p=value.toMap();
        if(p["reachable"].toBool() || (p["id"].toString()==m_localPeerId && m_showLocalOffline)) result.append(p);
    }
    return result;
}
QVariantMap SceneModel::lastEvent() const { return m_lastEvent; }
int SceneModel::textLength(const QString& text) const { return int(text.trimmed().toUcs4().size()); }
QString SceneModel::normalizedText(const QString& text) const { return text.trimmed(); }
bool SceneModel::textValid(const QString& name,const QString& text) const
{
    if(name!="setWip" && name!="note") return false;
    return isValidCommand({"validation",name=="setWip"?Action::SetWip:Action::Note,"target",text,false});
}
QString SceneModel::localPeerId() const { return m_localPeerId; }
void SceneModel::setLocalPeerId(const QString& id)
{
    if (id==m_localPeerId) return;
    m_localPeerId=id; emit localPeerIdChanged(); emit peersChanged();
}
void SceneModel::replaceSnapshot(const SessionState& state)
{
    if (!isValidSession(state)) { emit snapshotRequired(); return; }
    if (state.epoch==m_state.epoch && state.revision<m_state.revision) return;
    emit sceneReset(); m_state=state; m_lastEvent.clear();
    emit peersChanged(); emit lastEventChanged();
}
void SceneModel::applyEvent(const SessionEvent& e)
{
    if (e.epoch==m_state.epoch && e.revision<=m_state.revision) return;
    if (encodeEvent(e).isEmpty() || e.epoch!=m_state.epoch || e.revision!=m_state.revision+1
        || !m_state.peers.contains(e.actorId) || !m_state.peers.contains(e.targetId)) {
        emit snapshotRequired(); return;
    }
    const auto& old=m_state.peers[e.targetId];
    const bool moving=e.action==Action::Rope || e.action==Action::Lure;
    if ((!moving && e.finalX!=old.x) || (e.action!=Action::SetBusy && e.busy!=old.busy)) {
        emit snapshotRequired(); return;
    }
    auto next=m_state; auto& p=next.peers[e.targetId]; next.revision=e.revision;
    p.x=e.finalX; p.busy=e.busy;
    if (moving) p.pinned=true;
    if (e.action==Action::SetWip) p.wip=e.text;
    if (e.action==Action::AckInbox) p.inbox.clear();
    if (e.action==Action::Note || e.queued) p.inbox.append({e.eventId,e.action,e.text});
    if (!isValidSession(next)) { emit snapshotRequired(); return; }
    emit eventAboutToApply(); m_state=next;
    m_lastEvent=QJsonDocument::fromJson(encodeEvent(e)).object().toVariantMap();
    emit peersChanged(); emit lastEventChanged();
}
QString SceneModel::newRequestId() const {return QUuid::createUuid().toString(QUuid::WithoutBraces);}
void SceneModel::requestAction(const QString& name,const QString& target,const QString& text,bool busy,const QString& requestId)
{
    const auto request=requestId.isEmpty()?newRequestId():requestId;
    const auto reject=[&](const QString& code){emit actionRejected(code); emit requestFailed(request,code);};
    std::optional<Action> action;
    for (int i=0;i<11;++i) if (actionName(static_cast<Action>(i))==name) action=static_cast<Action>(i);
    if (!action || !m_state.peers.contains(m_localPeerId) || !m_state.peers.contains(target)) {
        reject("invalid"); return;
    }
    const bool self=*action==Action::SetBusy || *action==Action::SetWip || *action==Action::AckInbox;
    if (self!=(target==m_localPeerId)) { reject("forbidden"); return; }
    if (!m_transportConnected || !m_state.peers[m_localPeerId].reachable || (!self && !m_state.peers[target].reachable)) {
        reject("offline"); return;
    }
    if (!self && m_state.peers[target].busy && *action!=Action::Coffee && *action!=Action::Energy
        && *action!=Action::Note) { reject("busy"); return; }
    Command command{request,*action,target,text,busy};
    if (!isValidCommand(command)) { reject("invalid"); return; }
    emit actionRequested(*action,target,text.trimmed(),busy,request);
}
