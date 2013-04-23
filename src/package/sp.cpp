#include "sp.h"
#include "client.h"
#include "general.h"
#include "skill.h"
#include "standard-skillcards.h"
#include "engine.h"
#include "maneuvering.h"

class SPMoonSpearSkill: public WeaponSkill {
public:
    SPMoonSpearSkill(): WeaponSkill("sp_moonspear") {
        events << CardUsed << CardResponded;
    }

    virtual bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const{
        if (player->getPhase() != Player::NotActive)
            return false;

        CardStar card = NULL;
        if (event == CardUsed) {
            CardUseStruct card_use = data.value<CardUseStruct>();
            card = card_use.card;
        } else if (event == CardResponded) {
            card = data.value<CardResponseStruct>().m_card;
        }

        if (card == NULL || !card->isBlack()
            || (card->getHandlingMethod() != Card::MethodUse && card->getHandlingMethod() != Card::MethodResponse))
            return false;

        QList<ServerPlayer *> targets;
        foreach (ServerPlayer *tmp, room->getOtherPlayers(player)) {
            if (player->inMyAttackRange(tmp))
                targets << tmp;
        }
        if (targets.isEmpty()) return false;

        ServerPlayer *target = room->askForPlayerChosen(player, targets, objectName(), "@sp_moonspear", true);
        if (!target) return false;
        room->setEmotion(player, "weapon/moonspear");
        if (!room->askForCard(target, "jink", "@moon-spear-jink", QVariant(), Card::MethodResponse, player))
            room->damage(DamageStruct(objectName(), player, target));
        return false;
    }
};

SPMoonSpear::SPMoonSpear(Suit suit, int number)
    : Weapon(suit, number, 3)
{
    setObjectName("sp_moonspear");
}

class Jilei: public TriggerSkill {
public:
    Jilei(): TriggerSkill("jilei") {
        events << DamageInflicted;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *yangxiu, QVariant &data) const{
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *current = room->getCurrent();
        if (!current || current->isDead())
            return false;

        if (damage.from == NULL)
           return false;

        if (room->askForSkillInvoke(yangxiu, objectName(), data)) {
            QString choice = room->askForChoice(yangxiu, objectName(), "basic+equip+trick");
            room->broadcastSkillInvoke(objectName());

            room->setPlayerJilei(damage.from, choice);
            room->setPlayerFlag(damage.from, "jilei");

            LogMessage log;
            log.type = "#Jilei";
            log.from = damage.from;
            log.arg = choice;
            room->sendLog(log);

            if (damage.from->getMark("@jilei_" + choice) == 0)
                room->addPlayerMark(damage.from, "@jilei_" + choice);
        }

        return false;
    }
};

class JileiClear: public TriggerSkill {
public:
    JileiClear(): TriggerSkill("#jilei-clear") {
        events << EventPhaseChanging << Death;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent event, Room *room, ServerPlayer *target, QVariant &data) const{
        if (event == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to != Player::NotActive)
                return false;
        } else if (event == Death) {
            DeathStruct death = data.value<DeathStruct>();
            if (death.who != target || target != room->getCurrent())
                return false;
        }
        QList<ServerPlayer *> players = room->getAllPlayers();
        foreach (ServerPlayer *player, players) {
            if (player->hasFlag("jilei")) {
                room->setPlayerFlag(player, "-jilei");

                LogMessage log;
                log.type = "#JileiClear";
                log.from = player;
                room->sendLog(log);

                room->setPlayerMark(player, "@jilei_basic", 0);
                room->setPlayerMark(player, "@jilei_equip", 0);
                room->setPlayerMark(player, "@jilei_trick", 0);
                room->setPlayerJilei(player, "clear");
            }
        }

        return false;
    }
};

class Danlao: public TriggerSkill {
public:
    Danlao(): TriggerSkill("danlao") {
        events << TargetConfirmed << CardEffected;
    }

    virtual bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const{
        if (event == TargetConfirmed) {
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.to.length() <= 1 || !use.to.contains(player)
                || !use.card->isKindOf("TrickCard")
                || !room->askForSkillInvoke(player, objectName(), data))
                return false;

            player->tag["Danlao"] = use.card->toString();
            room->broadcastSkillInvoke(objectName());

            player->drawCards(1);
        } else {
            if (!player->isAlive() || !player->hasSkill(objectName()))
                return false;

            CardEffectStruct effect = data.value<CardEffectStruct>();
            if (player->tag["Danlao"].isNull() || player->tag["Danlao"].toString() != effect.card->toString())
                return false;

            player->tag["Danlao"] = QVariant(QString());

            LogMessage log;
            log.type = "#DanlaoAvoid";
            log.from = player;
            log.arg = effect.card->objectName();
            log.arg2 = objectName();
            room->sendLog(log);

            return true;
        }

        return false;
    }
};

Yongsi::Yongsi(): TriggerSkill("yongsi") {
    events << DrawNCards << EventPhaseStart;
    frequency = Compulsory;
}

int Yongsi::getKingdoms(ServerPlayer *yuanshu) const{
    QSet<QString> kingdom_set;
    Room *room = yuanshu->getRoom();
    foreach (ServerPlayer *p, room->getAlivePlayers())
        kingdom_set << p->getKingdom();

    return kingdom_set.size();
}

bool Yongsi::trigger(TriggerEvent event, Room *room, ServerPlayer *yuanshu, QVariant &data) const{
    if (event == DrawNCards) {
        int x = getKingdoms(yuanshu);
        data = data.toInt() + x;

        Room *room = yuanshu->getRoom();
        LogMessage log;
        log.type = "#YongsiGood";
        log.from = yuanshu;
        log.arg = QString::number(x);
        log.arg2 = objectName();
        room->sendLog(log);
        room->notifySkillInvoked(yuanshu, objectName());

        room->broadcastSkillInvoke("yongsi", x % 2 + 1);
    } else if (event == EventPhaseStart && yuanshu->getPhase() == Player::Discard) {
        int x = getKingdoms(yuanshu);
        LogMessage log;
        log.type = yuanshu->getCardCount(true) > x ? "#YongsiBad" : "#YongsiWorst";
        log.from = yuanshu;
        log.arg = QString::number(log.type == "#YongsiBad" ? x : yuanshu->getCardCount(true));
        log.arg2 = objectName();
        room->sendLog(log);
        room->notifySkillInvoked(yuanshu, objectName());
        if (x > 0)
            room->askForDiscard(yuanshu, "yongsi", x, x, false, true);
    }

    return false;
}

#include "standard-skillcards.h"
WeidiCard::WeidiCard() {
    target_fixed = true;
}

void WeidiCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const{
    if (!room->askForUseCard(source, "@jijiang", "@weidi-jijiang"))
        room->setPlayerFlag(source, "JijiangFailed");
}

class WeidiViewAsSkill: public ZeroCardViewAsSkill {
public:
    WeidiViewAsSkill(): ZeroCardViewAsSkill("weidi") {
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        JijiangViewAsSkill *jijiang = new JijiangViewAsSkill;
        jijiang->deleteLater();
        return jijiang->isEnabledAtPlay(player);
    }

    virtual bool isEnabledAtResponse(const Player *player, const QString &pattern) const{
        JijiangViewAsSkill *jijiang = new JijiangViewAsSkill;
        jijiang->deleteLater();
        return jijiang->isEnabledAtResponse(player, pattern);
    }

    virtual const Card *viewAs() const{
        return new WeidiCard;
    }
};

class Weidi: public GameStartSkill {
public:
    Weidi(): GameStartSkill("weidi") {
        frequency = Compulsory;
        view_as_skill = new WeidiViewAsSkill;
    }

    virtual void onGameStart(ServerPlayer *) const{
        return;
    }
};

class Yicong: public DistanceSkill {
public:
    Yicong(): DistanceSkill("yicong") {
    }

    virtual int getCorrect(const Player *from, const Player *to) const{
        int correct = 0;
        if (from->hasSkill(objectName()) && from->getHp() > 2)
            correct--;
        if (to->hasSkill(objectName()) && to->getHp() <= 2)
            correct++;

        return correct;
    }
};

class YicongEffect: public TriggerSkill {
public:
    YicongEffect(): TriggerSkill("#yicong_effect") {
        events << PostHpReduced << HpRecover;
    }

    virtual bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const{
        int hp = player->getHp();
        int index = 0;
        if (event == HpRecover) {
            RecoverStruct recover = data.value<RecoverStruct>();
            if (hp > 2 && hp - recover.recover <= 2)
                index = 1;
        } else if (event == PostHpReduced) {
            int reduce = 0;
            if (data.canConvert<DamageStruct>()) {
                DamageStruct damage = data.value<DamageStruct>();
                reduce = damage.damage;
            } else
                reduce = data.toInt();
            if (hp <= 2 && hp + reduce > 2)
                index = 2;
        }

        if (index > 0)
            room->broadcastSkillInvoke("yicong", index);
        return false;
    }
};

class Danji: public PhaseChangeSkill {
public:
    Danji(): PhaseChangeSkill("danji") { // What a silly skill!
        frequency = Wake;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return PhaseChangeSkill::triggerable(target)
               && target->getPhase() == Player::Start
               && target->getMark("danji") == 0
               && target->getHandcardNum() > target->getHp();
    }

    virtual bool onPhaseChange(ServerPlayer *guanyu) const{
        Room *room = guanyu->getRoom();
        ServerPlayer *the_lord = room->getLord();
        if (the_lord && (the_lord->getGeneralName() == "caocao" || the_lord->getGeneral2Name() == "caocao")) {
            room->notifySkillInvoked(guanyu, objectName());

            LogMessage log;
            log.type = "#DanjiWake";
            log.from = guanyu;
            log.arg = QString::number(guanyu->getHandcardNum());
            log.arg2 = QString::number(guanyu->getHp());
            room->sendLog(log);
            room->broadcastSkillInvoke(objectName());
            room->doLightbox("$DanjiAnimate", 5000);

            room->addPlayerMark(guanyu, "danji");
            if (room->changeMaxHpForAwakenSkill(guanyu))
                room->acquireSkill(guanyu, "mashu");
        }

        return false;
    }
};

YuanhuCard::YuanhuCard() {
    mute = true;
    will_throw = false;
    handling_method = Card::MethodNone;
}

bool YuanhuCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    if (!targets.isEmpty())
        return false;

    const Card *card = Sanguosha->getCard(subcards.first());
    const EquipCard *equip = qobject_cast<const EquipCard *>(card->getRealCard());
    int equip_index = static_cast<int>(equip->location());
    return to_select->getEquip(equip_index) == NULL;
}

void YuanhuCard::onUse(Room *room, const CardUseStruct &card_use) const{
    int index = -1;
    if (card_use.to.first() == card_use.from)
        index = 5;
    else if (card_use.to.first()->getGeneralName().contains("caocao"))
        index = 4;
    else {
        const Card *card = Sanguosha->getCard(card_use.card->getSubcards().first());
        if (card->isKindOf("Weapon"))
            index = 1;
        else if (card->isKindOf("Armor"))
            index = 2;
        else if (card->isKindOf("Horse"))
            index = 3;
    }
    room->broadcastSkillInvoke("yuanhu", index);
    SkillCard::onUse(room, card_use);
}

void YuanhuCard::onEffect(const CardEffectStruct &effect) const{
    ServerPlayer *caohong = effect.from;
    Room *room = caohong->getRoom();
    room->moveCardTo(this, caohong, effect.to, Player::PlaceEquip,
                     CardMoveReason(CardMoveReason::S_REASON_PUT, caohong->objectName(), "yuanhu", QString()));

    const Card *card = Sanguosha->getCard(subcards.first());

    LogMessage log;
    log.type = "$ZhijianEquip";
    log.from = effect.to;
    log.card_str = QString::number(card->getEffectiveId());
    room->sendLog(log);

    if (card->isKindOf("Weapon")) {
      QList<ServerPlayer *> targets;
      foreach (ServerPlayer *p, room->getAllPlayers()) {
          if (effect.to->distanceTo(p) == 1 && !p->isAllNude())
              targets << p;
      }
      if (!targets.isEmpty()) {
          ServerPlayer *to_dismantle = room->askForPlayerChosen(caohong, targets, "yuanhu", "@yuanhu-discard:" + effect.to->objectName());
          int card_id = room->askForCardChosen(caohong, to_dismantle, "hej", "yuanhu");
          room->throwCard(Sanguosha->getCard(card_id), to_dismantle, caohong);
      }
    } else if (card->isKindOf("Armor")) {
        effect.to->drawCards(1);
    } else if (card->isKindOf("Horse")) {
        RecoverStruct recover;
        recover.who = effect.from;
        room->recover(effect.to, recover);
    }
}

class YuanhuViewAsSkill: public OneCardViewAsSkill {
public:
    YuanhuViewAsSkill(): OneCardViewAsSkill("yuanhu") {
    }

    virtual bool isEnabledAtPlay(const Player *) const{
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const{
        return pattern == "@@yuanhu";
    }

    virtual bool viewFilter(const Card *to_select) const{
        return to_select->isKindOf("EquipCard");
    }

    virtual const Card *viewAs(const Card *originalcard) const{
        YuanhuCard *first = new YuanhuCard;
        first->addSubcard(originalcard->getId());
        first->setSkillName(objectName());
        return first;
    }
};

class Yuanhu: public PhaseChangeSkill {
public:
    Yuanhu(): PhaseChangeSkill("yuanhu") {
        view_as_skill = new YuanhuViewAsSkill;
    }

    virtual bool onPhaseChange(ServerPlayer *target) const{
        Room *room = target->getRoom();
        if (target->getPhase() == Player::Finish && !target->isNude())
            room->askForUseCard(target, "@@yuanhu", "@yuanhu-equip", -1, Card::MethodNone);
        return false;
    }
};

XuejiCard::XuejiCard() {
}

bool XuejiCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    if (targets.length() >= Self->getLostHp())
        return false;

    if (to_select == Self)
        return false;

    int range_fix = 0;
    if (Self->getWeapon() && Self->getWeapon()->getEffectiveId() == getEffectiveId()) {
        const Weapon *weapon = qobject_cast<const Weapon *>(Self->getWeapon()->getRealCard());
        range_fix += weapon->getRange() - 1;
    } else if (Self->getOffensiveHorse() && Self->getOffensiveHorse()->getEffectiveId() == getEffectiveId())
        range_fix += 1;

    return Self->distanceTo(to_select, range_fix) <= Self->getAttackRange();
}

void XuejiCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const{
    DamageStruct damage;
    damage.from = source;
    damage.reason = "xueji";

    foreach (ServerPlayer *p, targets) {
        damage.to = p;
        room->damage(damage);
    }
    foreach (ServerPlayer *p, targets) {
        if (p->isAlive())
            p->drawCards(1);
    }
}

class Xueji: public OneCardViewAsSkill {
public:
    Xueji(): OneCardViewAsSkill("xueji") {
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return player->getLostHp() > 0 && !player->isNude() && !player->hasUsed("XuejiCard");
    }

    virtual bool viewFilter(const Card *to_select) const{
        return to_select->isRed() && !Self->isJilei(to_select);
    }

    virtual const Card *viewAs(const Card *originalcard) const{
        XuejiCard *first = new XuejiCard;
        first->addSubcard(originalcard->getId());
        first->setSkillName(objectName());
        return first;
    }
};

class Huxiao: public TargetModSkill {
public:
    Huxiao(): TargetModSkill("huxiao") {
    }

    virtual int getResidueNum(const Player *from, const Card *) const{
        if (from->hasSkill(objectName()))
            return from->getMark(objectName());
        else
            return 0;
    }
};

class HuxiaoCount: public TriggerSkill {
public:
    HuxiaoCount(): TriggerSkill("#huxiao-count") {
        events << SlashMissed << EventPhaseChanging;
    }

    virtual bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const{
        if (event == SlashMissed) {
            if (player->getPhase() == Player::Play)
                room->addPlayerMark(player, "huxiao");
        } else if (event == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.from == Player::Play)
                if (player->getMark("huxiao") > 0)
                    room->setPlayerMark(player, "huxiao", 0);
        }

        return false;
    }
};

class HuxiaoClear: public TriggerSkill {
public:
    HuxiaoClear(): TriggerSkill("#huxiao-clear") {
        events << EventLoseSkill;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target && !target->hasSkill("huxiao") && target->getMark("huxiao") > 0;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const{
        room->setPlayerMark(player, "huxiao", 0);
        return false;
    }
};

class WujiCount: public TriggerSkill {
public:
    WujiCount(): TriggerSkill("#wuji-count") {
        events << PreDamageDone << EventPhaseChanging;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const{
        if (event == PreDamageDone) {
            DamageStruct damage = data.value<DamageStruct>();
            if (damage.from && damage.from->isAlive() && damage.from == room->getCurrent() && damage.from->getMark("wuji") == 0)
                room->addPlayerMark(damage.from, "wuji_damage", damage.damage);
        } else if (event == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to == Player::NotActive)
                if (player->getMark("wuji_damage") > 0)
                    room->setPlayerMark(player, "wuji_damage", 0);
        }

        return false;
    }
};

class Wuji: public PhaseChangeSkill {
public:
    Wuji(): PhaseChangeSkill("wuji") {
        frequency = Wake;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return PhaseChangeSkill::triggerable(target)
               && target->getPhase() == Player::Finish
               && target->getMark("wuji") == 0
               && target->getMark("wuji_damage") >= 3;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const{
        Room *room = player->getRoom();
        room->notifySkillInvoked(player, objectName());

        LogMessage log;
        log.type = "#WujiWake";
        log.from = player;
        log.arg = QString::number(player->getMark("wuji_damage"));
        log.arg2 = objectName();
        room->sendLog(log);

        room->broadcastSkillInvoke(objectName());
        room->doLightbox("$WujiAnimate", 4000);

        room->addPlayerMark(player, "wuji");

        if (room->changeMaxHpForAwakenSkill(player, 1)) {
            RecoverStruct recover;
            recover.who = player;
            room->recover(player, recover);

            room->detachSkillFromPlayer(player, "huxiao");
        }

        return false;
    }
};

class Baobian: public TriggerSkill {
public:
    Baobian(): TriggerSkill("baobian") {
        events << GameStart << HpChanged << MaxHpChanged << EventAcquireSkill << EventLoseSkill;
        frequency = Compulsory;
    }

    static void BaobianChange(Room *room, ServerPlayer *player, int hp, const QString &skill_name) {
        QStringList baobian_skills = player->tag["BaobianSkills"].toStringList();
        if (player->getHp() <= hp) {
            if (!baobian_skills.contains(skill_name)) {
                room->notifySkillInvoked(player, "baobian");
                if (player->getHp() == hp)
                    room->broadcastSkillInvoke("baobian", 4 - hp);
                room->acquireSkill(player, skill_name);
                baobian_skills << skill_name;
            }
        } else {
            room->detachSkillFromPlayer(player, skill_name);
            baobian_skills.removeOne(skill_name);
        }
        player->tag["BaobianSkills"] = QVariant::fromValue(baobian_skills);
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const{
        if (event == EventLoseSkill) {
            if (data.toString() == objectName()) {
                QStringList baobian_skills = player->tag["BaobianSkills"].toStringList();
                foreach (QString skill_name, baobian_skills)
                    room->detachSkillFromPlayer(player, skill_name);
                player->tag["BaobianSkills"] = QVariant();
            }
            return false;
        }

        if (!TriggerSkill::triggerable(player)) return false;

        BaobianChange(room, player, 1, "shensu");
        BaobianChange(room, player, 2, "paoxiao");
        BaobianChange(room, player, 3, "tiaoxin");
        return false;
    }
};

BifaCard::BifaCard() {
    mute = true;
    will_throw = false;
    handling_method = Card::MethodNone;
}

bool BifaCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    return targets.isEmpty() && to_select != Self;
}

void BifaCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &targets) const{
    ServerPlayer *target = targets.first();
    target->tag["BifaSource" + QString::number(getEffectiveId())] = QVariant::fromValue((PlayerStar)source);
    room->broadcastSkillInvoke("bifa", 1);
    target->addToPile("bifa", this, false);
}

class BifaViewAsSkill: public OneCardViewAsSkill {
public:
    BifaViewAsSkill(): OneCardViewAsSkill("bifa") {
    }

    virtual bool isEnabledAtPlay(const Player *) const{
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *, const QString &pattern) const{
        return pattern == "@@bifa";
    }

    virtual bool viewFilter(const Card *to_select) const{
        return !to_select->isEquipped();
    }

    virtual const Card *viewAs(const Card *originalcard) const{
        Card *card = new BifaCard;
        card->addSubcard(originalcard);
        return card;
    }
};

class Bifa: public TriggerSkill {
public:
    Bifa(): TriggerSkill("bifa") {
        events << EventPhaseStart;
        view_as_skill = new BifaViewAsSkill;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &) const{
        if (TriggerSkill::triggerable(player) && player->getPhase() == Player::Finish && !player->isKongcheng()) {
            room->askForUseCard(player, "@@bifa", "@bifa-remove", -1, Card::MethodNone);
        } else if (player->getPhase() == Player::RoundStart && player->getPile("bifa").length() > 0) {
            QList<int> bifa_list = player->getPile("bifa");

            while (!bifa_list.isEmpty()) {
                int card_id = bifa_list.last();
                ServerPlayer *chenlin = player->tag["BifaSource" + QString::number(card_id)].value<PlayerStar>();
                QList<int> ids;
                ids << card_id;
                room->fillAG(ids, player);
                const Card *cd = Sanguosha->getCard(card_id);
                QString pattern;
                if (cd->isKindOf("BasicCard"))
                    pattern = "BasicCard";
                else if (cd->isKindOf("TrickCard"))
                    pattern = "TrickCard";
                else if (cd->isKindOf("EquipCard"))
                    pattern = "EquipCard";
                QVariant data_for_ai = QVariant::fromValue(pattern);
                pattern.append("|.|.|hand");
                const Card *to_give = NULL;
                if (!player->isKongcheng() && chenlin && chenlin->isAlive())
                    to_give = room->askForCard(player, pattern, "@bifa-give", data_for_ai, Card::MethodNone, chenlin);
                if (chenlin && to_give) {
                    room->broadcastSkillInvoke(objectName(), 2);
                    chenlin->obtainCard(to_give, false);
                    player->obtainCard(cd, false);
                } else {
                    room->broadcastSkillInvoke(objectName(), 3);
                    CardMoveReason reason(CardMoveReason::S_REASON_REMOVE_FROM_PILE, QString(), objectName(), QString());
                    room->throwCard(cd, reason, NULL);
                    room->loseHp(player);
                }
                bifa_list.removeOne(card_id);
                room->clearAG(player);
                player->tag.remove("BifaSource" + QString::number(card_id));
            }
        }
        return false;
    }
};

SongciCard::SongciCard() {
    mute = true;
}

bool SongciCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    return targets.isEmpty() && to_select->getMark("@songci") == 0 && to_select->getHandcardNum() != to_select->getHp();
}

void SongciCard::onEffect(const CardEffectStruct &effect) const{
    int handcard_num = effect.to->getHandcardNum();
    int hp = effect.to->getHp();
    effect.to->gainMark("@songci");
    if (handcard_num > hp) {
        effect.to->getRoom()->broadcastSkillInvoke("songci", 2);
        effect.to->getRoom()->askForDiscard(effect.to, "songci", 2, 2, false, true);
    } else if (handcard_num < hp) {
        effect.to->getRoom()->broadcastSkillInvoke("songci", 1);
        effect.to->drawCards(2, "songci");
    }
}

class SongciViewAsSkill: public ZeroCardViewAsSkill {
public:
    SongciViewAsSkill(): ZeroCardViewAsSkill("songci") {
    }

    virtual const Card *viewAs() const{
        return new SongciCard;
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        if (player->getMark("@songci") == 0 && player->getHandcardNum() != player->getHp()) return true;
        foreach (const Player *sib, player->getSiblings())
            if (sib->getMark("@songci") == 0 && sib->getHandcardNum() != sib->getHp())
                return true;
        return false;
    }
};

class Songci: public TriggerSkill {
public:
    Songci(): TriggerSkill("songci") {
        events << Death;
        view_as_skill = new SongciViewAsSkill;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target && target->hasSkill(objectName());
    }

    virtual bool trigger(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        DeathStruct death = data.value<DeathStruct>();
        if (death.who != player) return false;
        foreach (ServerPlayer *p, room->getAllPlayers())
            if (p->getMark("@songci") > 0)
                room->setPlayerMark(p, "@songci", 0);
        return false;
    }
};

class Xiuluo: public PhaseChangeSkill {
public:
    Xiuluo(): PhaseChangeSkill("xiuluo") {
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return PhaseChangeSkill::triggerable(target)
               && target->getPhase() == Player::Start
               && !target->isKongcheng()
               && hasDelayedTrick(target);
    }

    virtual bool onPhaseChange(ServerPlayer *target) const{
        Room *room = target->getRoom();
        while (hasDelayedTrick(target) && !target->isKongcheng()) {
            QStringList suits;
            foreach (const Card *jcard, target->getJudgingArea()) {
                if (!suits.contains(jcard->getSuitString()))
                    suits << jcard->getSuitString();
            }

            const Card *card = room->askForCard(target, QString(".|%1|.|hand").arg(suits.join(",")),
                                                "@xiuluo", QVariant(), objectName());
            if (!card || !hasDelayedTrick(target)) break;
            room->broadcastSkillInvoke(objectName());

            QList<int> avail_list, other_list;
            foreach (const Card *jcard, target->getJudgingArea()) {
                if (jcard->isKindOf("YanxiaoCard")) continue;
                if (jcard->getSuit() == card->getSuit())
                    avail_list << jcard->getEffectiveId();
                else
                    other_list << jcard->getEffectiveId();
            }
            room->fillAG(avail_list + other_list, NULL, other_list);
            int id = room->askForAG(target, avail_list, false, objectName());
            room->clearAG();
            room->throwCard(id, NULL);
        }

        return false;
    }

private:
    static bool hasDelayedTrick(const ServerPlayer *target) {
        foreach (const Card *card, target->getJudgingArea())
            if (!card->isKindOf("YanxiaoCard")) return true;
        return false;
    }
};

class Shenwei: public DrawCardsSkill {
public:
    Shenwei(): DrawCardsSkill("#shenwei-draw") {
        frequency = Compulsory;
    }

    virtual int getDrawNum(ServerPlayer *player, int n) const{
        Room *room = player->getRoom();
        room->broadcastSkillInvoke("shenwei");
        room->notifySkillInvoked(player, "shenwei");
        LogMessage log;
        log.type = "#TriggerSkill";
        log.from = player;
        log.arg = "shenwei";
        room->sendLog(log);

        return n + 2;
    }
};

class ShenweiKeep: public MaxCardsSkill {
public:
    ShenweiKeep(): MaxCardsSkill("shenwei") {
    }

    virtual int getExtra(const Player *target) const{
        if (target->hasSkill(objectName()))
            return 2;
        else
            return 0;
    }
};

class Shenji: public TargetModSkill {
public:
    Shenji(): TargetModSkill("shenji") {
    }

    virtual int getExtraTargetNum(const Player *from, const Card *) const{
        if (from->hasSkill(objectName()) && from->getWeapon() == NULL)
            return 2;
        else
            return 0;
    }
};

class Xingwu: public TriggerSkill {
public:
    Xingwu(): TriggerSkill("xingwu") {
        events << PreCardUsed << CardResponded << EventPhaseStart << CardsMoveOneTime;
    }

    virtual bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const{
        if (event == PreCardUsed || event == CardResponded) {
            CardStar card = NULL;
            if (event == PreCardUsed)
                card = data.value<CardUseStruct>().card;
            else {
                CardResponseStruct response = data.value<CardResponseStruct>();
                if (response.m_isUse)
                   card = response.m_card;
            }
            if (card && card->getHandlingMethod() == Card::MethodUse) {
                int n = player->getMark(objectName());
                if (card->isBlack())
                    n |= 1;
                else if (card->isRed())
                    n |= 2;
                player->setMark(objectName(), n);
            }
        } else if (event == EventPhaseStart) {
            if (player->getPhase() == Player::Discard) {
                int n = player->getMark(objectName());
                bool red_avail = ((n & 2) == 0), black_avail = ((n & 1) == 0);
                if (player->isKongcheng() || (!red_avail && !black_avail))
                    return false;
                QString pattern = ".|.|.|hand";
                if (red_avail != black_avail)
                    pattern = QString("%1|%2").arg(pattern).arg(red_avail ? "red" : "black");
                const Card *card = room->askForCard(player, pattern, "@xingwu", QVariant(), Card::MethodNone);
                if (card) {
                    room->notifySkillInvoked(player, objectName());
                    room->broadcastSkillInvoke(objectName());

                    LogMessage log;
                    log.type = "#InvokeSkill";
                    log.from = player;
                    log.arg = objectName();
                    room->sendLog(log);

                    player->addToPile(objectName(), card);
                }
            } else if (player->getPhase() == Player::RoundStart) {
                player->setMark(objectName(), 0);
            }
        } else if (event == CardsMoveOneTime) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.to == player && move.to_place == Player::PlaceSpecial && player->getPile(objectName()).length() >= 3) {
                player->clearOnePrivatePile(objectName());
                QList<ServerPlayer *> males;
                foreach (ServerPlayer *p, room->getAlivePlayers()) {
                    if (p->isMale())
                        males << p;
                }
                if (males.isEmpty()) return false;

                ServerPlayer *target = room->askForPlayerChosen(player, males, objectName());
                room->damage(DamageStruct(objectName(), player, target, 2));

                if (!player->isAlive()) return false;
                QList<const Card *> equips = target->getEquips();
                if (!equips.isEmpty()) {
                    DummyCard *dummy = new DummyCard;
                    foreach (const Card *equip, equips)
                        dummy->addSubcard(equip);
                    room->throwCard(dummy, target, player);
                    delete dummy;
                }
            }
        }
        return false;
    }
};

class Luoyan: public TriggerSkill {
public:
    Luoyan(): TriggerSkill("luoyan") {
        events << CardsMoveOneTime << EventAcquireSkill << EventLoseSkill;
        frequency = Compulsory;
    }

    virtual bool triggerable(const ServerPlayer *target) const{
        return target != NULL;
    }

    virtual bool trigger(TriggerEvent event, Room *room, ServerPlayer *player, QVariant &data) const{
        if (event == EventLoseSkill && data.toString() == objectName()) {
            if (player->getAcquiredSkills().contains("tianxiang"))
                room->detachSkillFromPlayer(player, "tianxiang");
            if (player->getAcquiredSkills().contains("liuli"))
                room->detachSkillFromPlayer(player, "liuli");
        } else if (event == EventAcquireSkill && data.toString() == objectName()) {
            if (!player->getPile("xingwu").isEmpty()) {
                room->notifySkillInvoked(player, objectName());
                room->acquireSkill(player, "tianxiang");
                room->acquireSkill(player, "liuli");
            }
        }else if (event == CardsMoveOneTime && TriggerSkill::triggerable(player)) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.to == player && move.to_place == Player::PlaceSpecial && move.to_pile_name == "xingwu") {
                if (player->getPile("xingwu").length() == 1) {
                    room->notifySkillInvoked(player, objectName());
                    room->acquireSkill(player, "tianxiang");
                    room->acquireSkill(player, "liuli");
                }
            } else if (move.from == player && move.from_places.contains(Player::PlaceSpecial)
                       && move.from_pile_names.contains("xingwu")) {
                if (player->getPile("xingwu").isEmpty()) {
                    if (player->getAcquiredSkills().contains("tianxiang"))
                        room->detachSkillFromPlayer(player, "tianxiang");
                    if (player->getAcquiredSkills().contains("liuli"))
                        room->detachSkillFromPlayer(player, "liuli");
                }
            }
        }
        return false;
    }
};

SPCardPackage::SPCardPackage()
    : Package("sp_cards")
{
    (new SPMoonSpear)->setParent(this);
    skills << new SPMoonSpearSkill;

    type = CardPack;
}

ADD_PACKAGE(SPCard)

SPPackage::SPPackage()
    : Package("sp")
{
    General *yangxiu = new General(this, "yangxiu", "wei", 3);
    yangxiu->addSkill(new Jilei);
    yangxiu->addSkill(new JileiClear);
    yangxiu->addSkill(new Danlao);
    related_skills.insertMulti("jilei", "#jilei-clear");

    General *sp_diaochan = new General(this, "sp_diaochan", "qun", 3, false, true);
    sp_diaochan->addSkill("lijian");
    sp_diaochan->addSkill("biyue");

    General *gongsunzan = new General(this, "gongsunzan", "qun");
    gongsunzan->addSkill(new Yicong);
    gongsunzan->addSkill(new YicongEffect);
    related_skills.insertMulti("yicong", "#yicong_effect");

    General *yuanshu = new General(this, "yuanshu", "qun");
    yuanshu->addSkill(new Yongsi);
    yuanshu->addSkill(new Weidi);
    yuanshu->addSkill(new SPConvertSkill("yuanshu", "tw_yuanshu"));

    General *sp_sunshangxiang = new General(this, "sp_sunshangxiang", "shu", 3, false, true);
    sp_sunshangxiang->addSkill("jieyin");
    sp_sunshangxiang->addSkill("xiaoji");

    General *sp_pangde = new General(this, "sp_pangde", "wei", 4, true, true);
    sp_pangde->addSkill("mashu");
    sp_pangde->addSkill("mengjin");

    General *sp_guanyu = new General(this, "sp_guanyu", "wei", 4);
    sp_guanyu->addSkill("wusheng");
    sp_guanyu->addSkill(new Danji);

    General *shenlvbu1 = new General(this, "shenlvbu1", "god", 8, true, true);
    shenlvbu1->addSkill("mashu");
    shenlvbu1->addSkill("wushuang");

    General *shenlvbu2 = new General(this, "shenlvbu2", "god", 4, true, true);
    shenlvbu2->addSkill("mashu");
    shenlvbu2->addSkill("wushuang");
    shenlvbu2->addSkill(new Xiuluo);
    shenlvbu2->addSkill(new ShenweiKeep);
    shenlvbu2->addSkill(new Shenwei);
    shenlvbu2->addSkill(new Shenji);
    related_skills.insertMulti("shenwei", "#shenwei-draw");

    General *sp_caiwenji = new General(this, "sp_caiwenji", "wei", 3, false, true);
    sp_caiwenji->addSkill("beige");
    sp_caiwenji->addSkill("duanchang");

    General *sp_machao = new General(this, "sp_machao", "qun", 4, true, true);
    sp_machao->addSkill("mashu");
    sp_machao->addSkill("tieji");

    General *sp_jiaxu = new General(this, "sp_jiaxu", "wei", 3, true, true);
    sp_jiaxu->addSkill("wansha");
    sp_jiaxu->addSkill("luanwu");
    sp_jiaxu->addSkill("weimu");
    sp_jiaxu->addSkill("#@chaos-1");

    General *caohong = new General(this, "caohong", "wei");
    caohong->addSkill(new Yuanhu);

    General *guanyinping = new General(this, "guanyinping", "shu", 3, false);
    guanyinping->addSkill(new Xueji);
    guanyinping->addSkill(new Huxiao);
    guanyinping->addSkill(new HuxiaoCount);
    guanyinping->addSkill(new HuxiaoClear);
    guanyinping->addSkill(new Wuji);
    guanyinping->addSkill(new WujiCount);
    related_skills.insertMulti("wuji", "#wuji-count");
    related_skills.insertMulti("huxiao", "#huxiao-count");
    related_skills.insertMulti("huxiao", "#huxiao-clear");

    General *xiahouba = new General(this, "xiahouba", "shu");
    xiahouba->addSkill(new Baobian);

    General *chenlin = new General(this, "chenlin", "wei", 3);
    chenlin->addSkill(new Bifa);
    chenlin->addSkill(new Songci);

    General *erqiao = new General(this, "erqiao", "wu", 3, false);
    erqiao->addSkill(new Xingwu);
    erqiao->addSkill(new Luoyan);

    General *tw_diaochan = new General(this, "tw_diaochan", "qun", 3, false, true);
    tw_diaochan->addSkill("lijian");
    tw_diaochan->addSkill("biyue");

    General *tw_yuanshu = new General(this, "tw_yuanshu", "qun", 4, true, true);
    tw_yuanshu->addSkill("yongsi");
    tw_yuanshu->addSkill("weidi");

    General *tw_daqiao = new General(this, "tw_daqiao", "wu", 3, false, true);
    tw_daqiao->addSkill("guose");
    tw_daqiao->addSkill("liuli");

    General *tw_zhaoyun = new General(this, "tw_zhaoyun", "shu", 4, true, true);
    tw_zhaoyun->addSkill("longdan");

    General *tw_zhenji = new General(this, "tw_zhenji", "wei", 3, false, true);
    tw_zhenji->addSkill("qingguo");
    tw_zhenji->addSkill("luoshen");

    General *tw_lvbu = new General(this, "tw_lvbu", "qun", 4, true, true);
    tw_lvbu->addSkill("wushuang");

    General *tw_ganning = new General(this, "tw_ganning", "wu", 4, true, true);
    tw_ganning->addSkill("qixi");

    General *tw_machao = new General(this, "tw_machao", "shu", 4, true, true);
    tw_machao->addSkill("mashu");
    tw_machao->addSkill("tieji");

    General *tw_zhugeliang = new General(this, "tw_zhugeliang", "shu", 3, true, true);
    tw_zhugeliang->addSkill("guanxing");
    tw_zhugeliang->addSkill("kongcheng");

    /* General *tw_huangyueying = new General(this, "tw_huangyueying", "shu", 3, false, true);
    tw_huangyueying->addSkill("jizhi");
    tw_huangyueying->addSkill("qicai"); */

    General *tw_zhangliao = new General(this, "tw_zhangliao", "wei", 4, true, true);
    tw_zhangliao->addSkill("tuxi");

    General *tw_huanggai = new General(this, "tw_huanggai", "wu", 4, true, true);
    tw_huanggai->addSkill("kurou");

    General *tw_guojia = new General(this, "tw_guojia", "wei", 3, true, true);
    tw_guojia->addSkill("tiandu");
    tw_guojia->addSkill("yiji");

    General *tw_luxun = new General(this, "tw_luxun", "wu", 3, true, true);
    tw_luxun->addSkill("qianxun");
    tw_luxun->addSkill("lianying");

    General *wz_daqiao = new General(this, "wz_daqiao", "wu", 3, false, true);
    wz_daqiao->addSkill("guose");
    wz_daqiao->addSkill("liuli");

    General *wz_xiaoqiao = new General(this, "wz_xiaoqiao", "wu", 3, false, true);
    wz_xiaoqiao->addSkill("tianxiang");
    wz_xiaoqiao->addSkill("#tianxiang");
    wz_xiaoqiao->addSkill("hongyan");

    addMetaObject<WeidiCard>();
    addMetaObject<YuanhuCard>();
    addMetaObject<XuejiCard>();
    addMetaObject<BifaCard>();
    addMetaObject<SongciCard>();
}

ADD_PACKAGE(SP)

HegemonySPPackage::HegemonySPPackage()
    : Package("hegemony_sp")
{
    General *sp_heg_zhouyu = new General(this, "sp_heg_zhouyu", "wu", 3, true, true);
    sp_heg_zhouyu->addSkill("yingzi");
    sp_heg_zhouyu->addSkill("fanjian");
}

ADD_PACKAGE(HegemonySP)

