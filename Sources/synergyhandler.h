#ifndef SYNERGYHANDLER_H
#define SYNERGYHANDLER_H

#include <QObject>
#include "Widgets/ui_extended.h"
#include "Widgets/draftitemcounter.h"
#include "utility.h"


enum VisibleRace {V_MURLOC, V_DEMON, V_MECHANICAL, V_ELEMENTAL, V_BEAST, V_TOTEM, V_PIRATE, V_DRAGON, V_NUM_RACES};
enum VisibleType {V_MINION, V_SPELL, V_WEAPON, V_WEAPON_ALL, V_NUM_TYPES};
enum VisibleMechanics {V_DISCOVER_DRAW, V_TAUNT, /*V_RESTORE,*/
                       V_AOE, V_PING, V_DAMAGE_DESTROY, V_REACH,
                       V_OVERLOAD, V_JADE_GOLEM,
                       V_SECRET, V_FREEZE, V_DISCARD,
                       V_BATTLECRY, V_SILENCE, V_STEALTH,
                       V_DEATHRATTLE, V_DEATHRATTLE_GOOD_ALL,
                       V_TAUNT_GIVER, V_TOKEN, V_WINDFURY,
                       V_ATTACK_BUFF, V_HEALTH_BUFF, V_RETURN,
                       V_DIVINE_SHIELD, V_DIVINE_SHIELD_ALL,
                       V_ENRAGED_MINION, V_ENRAGED_ALL,
                       V_NUM_MECHANICS};


class SynergyHandler : public QObject
{
    Q_OBJECT
public:
    SynergyHandler(QObject *parent, Ui::Extended *ui);
    ~SynergyHandler();

//Variables
private:
    Ui::Extended *ui;
    QMap<QString, QList<QString>> synergyCodes;
    DraftItemCounter **raceCounters, **cardTypeCounters, **mechanicCounters;
    DraftItemCounter *manaCounter;
    QHBoxLayout *horLayoutCardTypes, *horLayoutMechanics1, *horLayoutMechanics2;

//Metodos
public:
    void updateCounters(DeckCard &deckCard);
    void getSynergies(DeckCard &deckCard, QMap<QString, int> &synergies, QStringList &mechanicIcons);
    void initSynergyCodes();
    void clearLists(bool keepCounters);
    int draftedCardsCount();
    void initCounters(QList<DeckCard> deckCardList);
    void setTransparency(Transparency transparency, bool mouseInApp);

private:
    void createDraftItemCounters();
    void deleteDraftItemCounters();

    void updateManaCounter(DeckCard &deckCard);
    void updateRaceCounters(DeckCard &deckCard);
    void updateCardTypeCounters(DeckCard &deckCard);
    void updateMechanicCounters(DeckCard &deckCard);

    void getCardTypeSynergies(DeckCard &deckCard, QMap<QString, int> &synergies);
    void getRaceSynergies(DeckCard &deckCard, QMap<QString, int> &synergies);
    void getMechanicSynergies(DeckCard &deckCard, QMap<QString, int> &synergies, QStringList &mechanicIcons);

private:
//public: //TODO
    bool isSpellGen(const QString &code);
    bool isWeaponGen(const QString &code, const QString &text);
    bool isMurlocGen(const QString &code);
    bool isDemonGen(const QString &code);
    bool isMechGen(const QString &code);
    bool isElementalGen(const QString &code);
    bool isBeastGen(const QString &code);
    bool isTotemGen(const QString &code);
    bool isPirateGen(const QString &code);
    bool isDragonGen(const QString &code);
    bool isDiscoverDrawGen(const QString &code);
    bool isTaunt(const QString &code, const QJsonArray &mechanics);
    bool isTauntGen(const QString &code, const QJsonArray &referencedTags);
    bool isAoeGen(const QString &code);
    bool isDamageMinionsGen(const QString &code, const QJsonArray &mechanics, const QJsonArray &referencedTags, const QString &text, const CardType &cardType, int attack);
    bool isDestroyGen(const QString &code);
    bool isPingGen(const QString &code, const QJsonArray &mechanics, const QJsonArray &referencedTags,
                   const QString &text, const CardType &cardType, int attack);
    bool isReachGen(const QString &code, const QJsonArray &mechanics, const QJsonArray &referencedTags, const QString &text, const CardType &cardType, int attack);
    bool isEnrageMinion(const QString &code, const QJsonArray &mechanics);
    bool isEnrageGen(const QString &code, const QJsonArray &referencedTags);
    bool isOverload(const QString &code);
    bool isJadeGolemGen(const QString &code, const QJsonArray &mechanics, const QJsonArray &referencedTags);
    bool isSecretGen(const QString &code, const QJsonArray &mechanics);
    bool isFreezeGen(const QString &code, const QJsonArray &mechanics, const QJsonArray &referencedTags, const QString &text);
    bool isDiscardGen(const QString &code, const QString &text);
    bool isDeathrattleMinion(const QString &code, const QJsonArray &mechanics, const CardType &cardType);
    bool isDeathrattleGoodAll(const QString &code, const QJsonArray &mechanics, const QJsonArray &referencedTags);
    bool isBattlecryMinion(const QString &code, const QJsonArray &mechanics, const CardType &cardType);
    bool isSilenceOwnGen(const QString &code, const QJsonArray &mechanics, const QJsonArray &referencedTags);
    bool isTauntGiverGen(const QString &code);
    bool isTokenGen(const QString &code, const QString &text);
    bool isWindfuryMinion(const QString &code, const QJsonArray &mechanics, const CardType &cardType);
    bool isAttackBuffGen(const QString &code, const QString &text);
    bool isHealthBuffGen(const QString &code, const QString &text);
    bool isReturnGen(const QString &code, const QString &text);
    bool isStealthGen(const QString &code, const QJsonArray &mechanics);
    bool isDivineShield(const QString &code, const QJsonArray &mechanics);
    bool isDivineShieldGen(const QString &code, const QJsonArray &referencedTags);

    bool isMurlocSyn(const QString &code);
    bool isDemonSyn(const QString &code);
    bool isMechSyn(const QString &code);
    bool isElementalSyn(const QString &code);
    bool isBeastSyn(const QString &code);
    bool isTotemSyn(const QString &code);
    bool isPirateSyn(const QString &code);
    bool isDragonSyn(const QString &code);
    bool isSpellSyn(const QString &code);
    bool isWeaponSyn(const QString &code);
    bool isWeaponAllSyn(const QString &code, const QString &text);
    bool isEnrageMinionSyn(const QString &code);
    bool isEnrageAllSyn(const QString &code, const QString &text);
    bool isOverloadSyn(const QString &code, const QString &text);
    bool isPingSyn(const QString &code);
    bool isAoeSyn(const QString &code);
    bool isTauntSyn(const QString &code);
    bool isSecretSyn(const QString &code, const QJsonArray &referencedTags);
    bool isFreezeSyn(const QString &code, const QJsonArray &referencedTags, const QString &text);
    bool isDiscardSyn(const QString &code, const QString &text);
    bool isDeathrattleSyn(const QString &code);
    bool isDeathrattleGoodAllSyn(const QString &code);
    bool isBattlecrySyn(const QString &code, const QJsonArray &referencedTags);
    bool isSilenceOwnSyn(const QString &code, const QJsonArray &mechanics);
    bool isTauntGiverSyn(const QString &code, const QJsonArray &mechanics, int attack, const CardType &cardType);
    bool isTokenSyn(const QString &code, const QString &text);
    bool isWindfurySyn(const QString &code);
    bool isAttackBuffSyn(const QString &code);
    bool isHealthBuffSyn(const QString &code);
    bool isReturnSyn(const QString &code, const QJsonArray &mechanics, const CardType &cardType, const QString &text);
    bool isStealthSyn(const QString &code);
    bool isDivineShieldSyn(const QString &code);
    bool isDivineShieldAllSyn(const QString &code);

signals:
    void pLog(QString line);
    void pDebug(QString line, DebugLevel debugLevel=Normal, QString file="SynergyHandler");
};

#endif // SYNERGYHANDLER_H