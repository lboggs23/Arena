#include "drafthandler.h"
#include "themehandler.h"
#include <QtConcurrent/QtConcurrent>
#include <QtWidgets>

DraftHandler::DraftHandler(QObject *parent, Ui::Extended *ui, DeckHandler *deckHandler, ArenaHandler *arenaHandler) : QObject(parent)
{
    this->ui = ui;
    this->deckHandler = deckHandler;
    this->arenaHandler = arenaHandler;
    this->deckRatingHA = this->deckRatingLF = 0;
    this->deckRatingHSR = 0;
    this->numCaptured = 0;
    this->extendedCapture = false;
    this->resetTwitchScores = true;
    this->drafting = false;
    this->heroDrafting = false;
    this->capturing = false;
    this->findingFrame = false;
    this->stopLoops = true;
    this->transparency = Opaque;
    this->draftHeroWindow = nullptr;
    this->draftScoreWindow = nullptr;
    this->draftMechanicsWindow = nullptr;
    this->synergyHandler = nullptr;
    this->mouseInApp = false;
    this->draftMethodHA = false;
    this->draftMethodLF = true;
    this->draftMethodHSR = false;
    this->draftMethodAvgScore = LightForge;
    this->twitchHandler = nullptr;
    this->multiclassArena = false;
    this->learningMode = false;
    this->showDrops = true;
    this->cardsIncludedWinratesMap = nullptr;
    this->cardsIncludedDecksMap = nullptr;
    this->cardsPlayedWinratesMap = nullptr;
    this->screenIndex = -1;
    this->screenScale = QPointF(1,1);
    this->needSaveCardHist = false;

    for(int i=0; i<3; i++)
    {
        screenRects[i] = cv::Rect(0,0,0,0);
        cardDetected[i] = false;
    }

    createScoreItems();
    createSynergyHandler();
    completeUI();

    connect(&futureFindScreenRects, SIGNAL(finished()), this, SLOT(finishFindScreenRects()));
}

DraftHandler::~DraftHandler()
{
    deleteDraftHeroWindow();
    deleteDraftScoreWindow();
    deleteDraftMechanicsWindow();
    deleteTwitchHandler();
    if(synergyHandler != nullptr)  delete synergyHandler;
}


void DraftHandler::createScoreItems()
{
    int width = 80;
    lavaButton = new LavaButton(ui->tabDraft, 3, 5.5);
    lavaButton->setFixedHeight(width);
    lavaButton->setFixedWidth(width);
    lavaButton->reset();
    lavaButton->setToolTip("Deck weight");
    lavaButton->hide();

    scoreButtonLF = new ScoreButton(ui->tabDraft, Score_LightForge);
    scoreButtonLF->setFixedHeight(width);
    scoreButtonLF->setFixedWidth(width);
    scoreButtonLF->setScore(0, 0);
    scoreButtonLF->setToolTip("LightForge deck average");
    scoreButtonLF->hide();

    scoreButtonHA = new ScoreButton(ui->tabDraft, Score_HearthArena);
    scoreButtonHA->setFixedHeight(width);
    scoreButtonHA->setFixedWidth(width);
    scoreButtonHA->setScore(0, 0);
    scoreButtonHA->setToolTip("HearthArena deck average");
    scoreButtonHA->hide();

    scoreButtonHSR = new ScoreButton(ui->tabDraft, Score_HSReplay);
    scoreButtonHSR->setFixedHeight(width);
    scoreButtonHSR->setFixedWidth(width);
    scoreButtonHSR->setScore(0, 0);
    scoreButtonHSR->setToolTip("HSReplay winrate deck average");
    scoreButtonHSR->hide();

    QHBoxLayout *scoresLayout = new QHBoxLayout();
    scoresLayout->addWidget(lavaButton);
    scoresLayout->addWidget(scoreButtonLF);
    scoresLayout->addWidget(scoreButtonHA);
    scoresLayout->addWidget(scoreButtonHSR);

    ui->draftVerticalLayout->addLayout(scoresLayout);
    ui->draftVerticalLayout->addSpacing(10);
}


void DraftHandler::createSynergyHandler()
{
    this->synergyHandler = new SynergyHandler(this->parent(),ui);
    connect(synergyHandler, SIGNAL(itemEnter(QList<SynergyCard>&,QRect&,int,int)),
            this, SIGNAL(itemEnter(QList<SynergyCard>&,QRect&,int,int)));
    connect(synergyHandler, SIGNAL(itemLeave()),
            this, SIGNAL(itemLeave()));
    connect(synergyHandler, SIGNAL(pLog(QString)),
            this, SIGNAL(pLog(QString)));
    connect(synergyHandler, SIGNAL(pDebug(QString,DebugLevel,QString)),
            this, SIGNAL(pDebug(QString,DebugLevel,QString)));
}


SynergyHandler * DraftHandler::getSynergyHandler()
{
    return this->synergyHandler;
}


void DraftHandler::completeUI()
{
    comboBoxCard[0] = ui->comboBoxCard1;
    comboBoxCard[1] = ui->comboBoxCard2;
    comboBoxCard[2] = ui->comboBoxCard3;
    labelLFscore[0] = ui->labelLFscore1;
    labelLFscore[1] = ui->labelLFscore2;
    labelLFscore[2] = ui->labelLFscore3;
    labelHAscore[0] = ui->labelHAscore1;
    labelHAscore[1] = ui->labelHAscore2;
    labelHAscore[2] = ui->labelHAscore3;
    labelHSRscore[0] = ui->labelHSRscore1;
    labelHSRscore[1] = ui->labelHSRscore2;
    labelHSRscore[2] = ui->labelHSRscore3;

    for(int i=0; i<3; i++)
    {
        comboBoxCard[i]->setFocusPolicy(Qt::NoFocus);
    }

    connect(ui->refreshDraftButton, SIGNAL(clicked(bool)),
                this, SLOT(refreshCapturedCards()));

    setPremium(false);
}


void DraftHandler::setMulticlassArena(bool multiclassArena)
{
    this->multiclassArena = multiclassArena;
}


void DraftHandler::setPremium(bool premium)
{
    if(drafting)    return;
    this->patreonVersion = premium;

    if(premium)
    {
        ui->labelDeckScore->show();
    }
    else
    {
        ui->labelDeckScore->hide();
    }

    updateScoresVisibility();
}


void DraftHandler::connectAllComboBox()
{
    for(int i=0; i<3; i++)
    {
        connect(comboBoxCard[i], SIGNAL(currentIndexChanged(int)),
                this, SLOT(comboBoxChanged()));
        comboBoxCard[i]->setEnabled(true);
    }
    ui->refreshDraftButton->setEnabled(true);
}


void DraftHandler::clearAndDisconnectAllComboBox()
{
    for(int i=0; i<3; i++)
    {
        comboBoxCard[i]->setEnabled(false);
        clearAndDisconnectComboBox(i);
    }
    ui->refreshDraftButton->setEnabled(false);
}


void DraftHandler::clearAndDisconnectComboBox(int index)
{
    disconnect(comboBoxCard[index], nullptr, nullptr, nullptr);
    comboBoxCard[index]->clear();
}


void DraftHandler::setArenaSets(QStringList arenaSets)
{
    this->arenaSets = arenaSets;
}


QStringList DraftHandler::getAllArenaCodes()
{
    QStringList codeList;

    for(const QString &set: arenaSets)  codeList.append(Utility::getSetCodes(set, true, true));
    return codeList;
}


QStringList DraftHandler::getAllHeroCodes()
{
    return heroCodesList;
}


void DraftHandler::initHearthArenaTiers(const QString &heroString, const bool multiClassDraft)
{
    hearthArenaTiers.clear();

    QFile jsonFile(Utility::extraPath() + "/hearthArena.json");
    jsonFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonFile.readAll());
    jsonFile.close();

    if(multiClassDraft)
    {
        QJsonObject heroJsonObject = jsonDoc.object().value(heroString).toObject();
        QJsonObject othersJsonObject[NUM_HEROS-1];
        for(int j=0, i=0; j<NUM_HEROS; j++)
        {
            if(heroString != Utility::classOrder2classUL_ULName(j))
            {
                othersJsonObject[i++] = jsonDoc.object().value(Utility::classOrder2classUL_ULName(j)).toObject();
            }
        }
        for(const QString &code: lightForgeTiers.keys())
        {
            QString name = Utility::cardEnNameFromCode(code);
            if(heroJsonObject.contains(name))   hearthArenaTiers[code] = heroJsonObject.value(name).toInt();
            else
            {
                for(int i=0; i<(NUM_HEROS-1); i++)
                {
                    if(othersJsonObject[i].contains(name))
                    {
                        hearthArenaTiers[code] = othersJsonObject[i].value(name).toInt();
                        break;
                    }
                }
            }

            if(hearthArenaTiers[code] == 0)  emit pDebug("HearthArena missing: " + name);
        }
    }
    else
    {
        QJsonObject jsonNamesObject = jsonDoc.object().value(heroString).toObject();
        for(const QString &code: lightForgeTiers.keys())
        {
            QString name = Utility::cardEnNameFromCode(code);
            int score = jsonNamesObject.value(name).toInt();
            hearthArenaTiers[code] = score;
            if(score == 0)  emit pDebug("HearthArena missing: " + name);
        }
        emit pDebug("HearthArena Cards: " + QString::number(jsonNamesObject.count()));
    }
}


void DraftHandler::loadCardHist(QString classUName)
{
    QString code;
    int type, rows, cols;
    bool continuous;

    QFile file(Utility::histogramsPath() + "/" + classUName + HISTOGRAM_EXT);
    if(!file.open(QIODevice::ReadOnly))
    {
        emit pDebug("ERROR: Cannot open " + file.fileName(), DebugLevel::Error);
        return;
    }
    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_5_5);

    while(!in.atEnd())
    {
        in >> code >> type >> rows >> cols >> continuous;
        cardsHist[code] = cv::Mat(rows, cols, type);
        cv::Mat &mat = cardsHist[code];

        if(continuous)
        {
            size_t const dataSize = rows * cols * mat.elemSize();
            in.readRawData(reinterpret_cast<char*>(mat.ptr()), dataSize);
        }
        else
        {
            size_t const rowSize(cols * mat.elemSize());
            for(int i=0; i<rows; i++)   in.readRawData(reinterpret_cast<char*>(mat.ptr(i)), rowSize);
        }
    }
    file.close();
}


void DraftHandler::saveCardHist(const bool multiClassDraft)
{
    //Build codesByClass
    QMap<CardClass, QStringList> codesByClass;
    for(const QString &code: lightForgeTiers.keys())
    {
        QList<CardClass> cardClassList = Utility::getClassFromCode(code);
        if(multiClassDraft)
        {
            for(const CardClass &cardClass: cardClassList)
            {
                if(cardClass == INVALID_CLASS)  emit pDebug("WARNING: saveCardHist found INVALID_CLASS: " + code);
                else                            codesByClass[cardClass].append(code);
            }
        }
        else if(cardClassList.contains(NEUTRAL))    codesByClass[NEUTRAL].append(code);
        else if(cardClassList.contains(arenaHero))  codesByClass[arenaHero].append(code);
    }

    //Save
    for(const CardClass &cardClass: codesByClass.keys())
    {
        QString classUName = Utility::classEnum2classUName(cardClass);
        QFileInfo fi(Utility::histogramsPath() + "/" + classUName + HISTOGRAM_EXT);
        if(fi.exists())
        {
            emit pDebug("Save Arena Hists SKIP (" + classUName + "): " + QString::number(codesByClass[cardClass].count()));
        }
        else
        {
            int type, rows, cols;
            bool continuous;

            QFile file(Utility::histogramsPath() + "/" + classUName + HISTOGRAM_EXT);
            if(!file.open(QIODevice::WriteOnly))
            {
                emit pDebug("ERROR: Cannot open " + file.fileName(), DebugLevel::Error);
                return;
            }
            QDataStream out(&file);
            out.setVersion(QDataStream::Qt_5_5);

            for(QString code: codesByClass[cardClass])
            {
                for(int i=0; i<2; i++)
                {
                    if(i==1)    code += "_premium";
                    if(!cardsHist.contains(code))
                    {
                        emit pDebug("WARNING: saveCardHist " + classUName + ": " + code + " not found.");
                        continue;
                    }

                    cv::Mat &mat = cardsHist[code];
                    type = mat.type();
                    rows = mat.rows;
                    cols = mat.cols;
                    continuous = mat.isContinuous();
                    out << code << type << rows << cols << continuous;

                    if(continuous)
                    {
                        size_t const dataSize = rows * cols * mat.elemSize();
                        out.writeRawData(reinterpret_cast<char const*>(mat.ptr()), dataSize);
                    }
                    else
                    {
                        size_t const rowSize(cols * mat.elemSize());
                        for(int i=0; i<rows; i++)   out.writeRawData(reinterpret_cast<char const*>(mat.ptr(i)), rowSize);
                    }
                }
            }
            file.close();
            emit pDebug("Save Arena Hists SAVED (" + classUName + "): " + QString::number(codesByClass[cardClass].count()));
        }
    }

    needSaveCardHist = false;
}


void DraftHandler::addCardHist(QString code, bool premium, bool isHero)
{
    //Evitamos golden cards de cartas no colleccionables
    if(premium && !Utility::getCardAttribute(code, "collectible").toBool()) return;

    QString fileNameCode = premium?(code + "_premium"): code;
    //Puede ocurrir con dual class cards en multiclassArena.
    if(cardsHist.contains(fileNameCode))    return;

    QFileInfo cardFile(Utility::hscardsPath() + "/" + fileNameCode + ".png");
    if(cardFile.exists())
    {
        cv::MatND histBase = getHist(fileNameCode);
        if(!histBase.empty())   cardsHist[fileNameCode] = histBase;
    }
    //cardsDownloading no puede contener duplicados, puede ocurrir con dual class cards en multiclassArena.
    else if(!cardsDownloading.contains(fileNameCode))
    {
        //La bajamos de github/hearthSim
        emit checkCardImage(fileNameCode, isHero);
        cardsDownloading.append(fileNameCode);
    }
}


void DraftHandler::processCardHist(QStringList &codes)
{
    for(const QString &code: codes)
    {
        addCardHist(code, false);
        addCardHist(code, true);
    }
}


bool DraftHandler::initCardHist(QMap<CardClass, QStringList> &codesByClass)
{
    bool processed = false;
    for(const CardClass &cardClass: codesByClass.keys())
    {
        QString classUName = Utility::classEnum2classUName(cardClass);
        QFileInfo fi(Utility::histogramsPath() + "/" + classUName + HISTOGRAM_EXT);
        if(fi.exists())
        {
            loadCardHist(classUName);
            emit pDebug("Load Arena Hists (" + classUName + "): " + QString::number(cardsHist.count()) + " hists.");
        }
        else
        {
            processCardHist(codesByClass[cardClass]);
            emit pDebug("Process Arena Hists (" + classUName + "): " + QString::number(cardsHist.count()) + " hists.");
            processed = true;
        }
    }

    return processed;
}


void DraftHandler::initLightForgeTiers(const bool multiClassDraft, QMap<CardClass, QStringList> &codesByClass)
{
    lightForgeTiers.clear();

    for(const QString &code: getAllArenaCodes())
    {
        QList<CardClass> cardClassList = Utility::getClassFromCode(code);
        if(multiClassDraft || cardClassList.contains(NEUTRAL) || cardClassList.contains(arenaHero))
        {
            lightForgeTiers[code] = 0;

            if(multiClassDraft)
            {
                for(const CardClass &cardClass: cardClassList)
                {
                    if(cardClass == INVALID_CLASS)  emit pDebug("WARNING: initLightForgeTiers found INVALID_CLASS: " + code);
                    else                            codesByClass[cardClass].append(code);
                }
            }
            else if(cardClassList.contains(NEUTRAL))        codesByClass[NEUTRAL].append(code);
            else/* if(cardClassList.contains(arenaHero))*/  codesByClass[arenaHero].append(code);
        }
    }
    emit pDebug("Arena Cards: " + QString::number(lightForgeTiers.count()));
    for(const CardClass &cardClass: codesByClass.keys())
    {
        QString classUName = Utility::classEnum2classUName(cardClass);
        emit pDebug("-- (" + classUName + "): " + QString::number(codesByClass[cardClass].count()));
    }
}


void DraftHandler::initCodesAndHistMaps(QString hero, bool skipScreenSettings)
{
    cardsDownloading.clear();
    cardsHist.clear();
    needSaveCardHist = false;

    if(heroDrafting)
    {
        QTimer::singleShot(1000, this, SLOT(newFindScreenLoop()));

        for(const QString &code: heroCodesList)     addCardHist(code, false, true);
    }
    else //if(drafting) ||Build mechanics window
    {
        QTimer::singleShot(1000, this, [=] () {newFindScreenLoop(skipScreenSettings);});

        QMap<CardClass, QStringList> codesByClass;
        initLightForgeTiers(multiclassArena, codesByClass);
        if(drafting)    needSaveCardHist = initCardHist(codesByClass);
        initHearthArenaTiers(Utility::classLogNumber2classUL_ULName(hero), multiclassArena);
        synergyHandler->initSynergyCodes();
    }

    //Wait for cards
    if(drafting || heroDrafting)
    {
        if(cardsDownloading.isEmpty())
        {
            if(needSaveCardHist)    saveCardHist(multiclassArena);
            newCaptureDraftLoop();
        }
        else
        {
            emit startProgressBar(cardsDownloading.count(), "Downloading cards...");
            emit downloadStarted();
        }
    }
}


void DraftHandler::reHistDownloadedCardImage(const QString &fileNameCode, bool missingOnWeb)
{
    if(!cardsDownloading.contains(fileNameCode)) return; //No forma parte del drafting

    if(!fileNameCode.isEmpty() && !missingOnWeb)
    {
        cv::MatND histBase = getHist(fileNameCode);
        if(!histBase.empty())   cardsHist[fileNameCode] = histBase;
    }
    cardsDownloading.removeOne(fileNameCode);
    emit advanceProgressBar(cardsDownloading.count(), fileNameCode.split("_premium").first() + " downloaded");
    if(cardsDownloading.isEmpty())
    {
        if(needSaveCardHist)    saveCardHist(multiclassArena);
        emit showMessageProgressBar("All cards downloaded");
        emit downloadEnded();
        newCaptureDraftLoop();
    }
}


void DraftHandler::resetTab(bool alreadyDrafting)
{
    clearAndDisconnectAllComboBox();
    for(int i=0; i<3; i++)
    {
        clearScore(labelLFscore[i], LightForge);
        clearScore(labelHAscore[i], HearthArena);
        clearScore(labelHSRscore[i], HSReplay);
        draftCards[i].setCode("");
        draftCards[i].draw(comboBoxCard[i]);
        comboBoxCard[i]->setCurrentIndex(0);
    }

    if(!alreadyDrafting)
    {
        //SizePreDraft
        QMainWindow *mainWindow = static_cast<QMainWindow*>(parent());
        QSettings settings("Arena Tracker", "Arena Tracker");
        settings.setValue("size", mainWindow->size());

        //Show Tab
        ui->tabWidget->insertTab(0, ui->tabDraft, QIcon(ThemeHandler::tabArenaFile()), "");
        ui->tabWidget->setTabToolTip(0, "Draft");

        //Reset scores
        synergyHandler->setHidden(!patreonVersion);
        lavaButton->setHidden(!patreonVersion);
        scoreButtonHA->setEnabled(false);
        scoreButtonLF->setEnabled(false);
        scoreButtonHSR->setEnabled(false);
        lavaButton->setEnabled(false);
        lavaButton->reset();
        updateDeckScore();//Basicamente para updateLabelDeckScore
        updateScoresVisibility();
        updateAvgScoresVisibility();

        //SizeDraft
        QSize sizeDraft = settings.value("sizeDraft", QSize(350, 400)).toSize();
        mainWindow->resize(sizeDraft);
        emit calculateMinimumWidth();
    }

    ui->tabWidget->setCurrentWidget(ui->tabDraft);
}


void DraftHandler::clearLists(bool keepCounters)
{
    clearAndDisconnectAllComboBox();
    synergyHandler->clearLists(keepCounters);//keepCounters = beginDraft
    hearthArenaTiers.clear();
    lightForgeTiers.clear();
    cardsHist.clear();
    needSaveCardHist = false;

    if(!keepCounters)//endDraft
    {
        deckRatingHA = deckRatingLF = 0;
        deckRatingHSR = 0;
    }

    for(int i=0; i<3; i++)
    {
        screenRects[i]=cv::Rect(0,0,0,0);
        cardDetected[i] = false;
        draftCardMaps[i].clear();
        bestMatchesMaps[i].clear();
    }

    screenIndex = -1;
    screenScale = QPointF(1,1);
    numCaptured = 0;
    extendedCapture = false;
    resetTwitchScores = true;
    stopLoops = true;
}


void DraftHandler::enterArena()
{
    showOverlay();

    if(drafting)
    {
        if(!screenFound())  QTimer::singleShot(1000, this, SLOT(newFindScreenLoop()));
        else if(draftCards[0].getCode().isEmpty())
        {
            this->extendedCapture = false;
            this->resetTwitchScores = true;
            newCaptureDraftLoop(true);
        }
    }
}


void DraftHandler::leaveArena()
{
    stopLoops = true;

    if(draftScoreWindow != nullptr)        draftScoreWindow->hide();
    if(draftMechanicsWindow != nullptr)    draftMechanicsWindow->hide();

    if(drafting)
    {
        if(capturing)
        {
            this->numCaptured = 0;

            //Clear guessed cards
            for(int i=0; i<3; i++)
            {
                cardDetected[i] = false;
                draftCardMaps[i].clear();
                bestMatchesMaps[i].clear();
            }
        }
    }
    else if(heroDrafting)   endHeroDraft();
}


void DraftHandler::beginDraft(QString hero, QList<DeckCard> deckCardList, bool skipScreenSettings)
{
    deleteDraftMechanicsWindow();

    if(heroDrafting)
    {
        saveTemplateSettings();
        endHeroDraft();
    }

    bool alreadyDrafting = drafting;
    int heroInt = hero.toInt();
    if(heroInt<1 || heroInt>NUM_HEROS)
    {
        emit pDebug("Begin draft of unknown hero: " + hero, DebugLevel::Error);
        emit pLog(tr("Draft: ERROR: Started draft of unknown hero ") + hero);
        return;
    }
    else
    {
        emit pDebug("Begin draft. Heroe: " + hero);
        emit pLog(tr("Draft: New draft started."));
    }

    //Set updateTime in log / Hide card Window
    emit draftStarted();

    clearLists(true);

    this->arenaHero = Utility::classLogNumber2classEnum(hero);
    this->drafting = true;
    this->justPickedCard = "";

    initCodesAndHistMaps(hero, skipScreenSettings);
    resetTab(alreadyDrafting);
    initSynergyCounters(deckCardList);
    createTwitchHandler();
}


void DraftHandler::continueDraft()
{
    if(!drafting && arenaHero != INVALID_CLASS)
    {
        QString heroLog = Utility::classEnum2classLogNumber(arenaHero);
        arenaHandler->newArena(heroLog);
        beginDraft(heroLog, deckHandler->getDeckCardList());
    }
    else
    {
        emit pDebug("No continue draft because already drafting or no hero.", DebugLevel::Warning);
    }
}


void DraftHandler::createTwitchHandler()
{
    deleteTwitchHandler();

    if(TwitchHandler::isWellConfigured())
    {
        this->twitchHandler = new TwitchHandler(this);
        connect(twitchHandler, SIGNAL(connectionOk(bool)),
                this, SLOT(twitchHandlerConnectionOk(bool)));
        connect(twitchHandler, SIGNAL(voteUpdate(int,int,int,QString)),
                this, SLOT(twitchHandlerVoteUpdate(int,int,int,QString)));
        connect(twitchHandler, SIGNAL(showMessageProgressBar(QString,int)),
                this, SIGNAL(showMessageProgressBar(QString,int)));
        connect(twitchHandler, SIGNAL(pLog(QString)),
                this, SIGNAL(pLog(QString)));
        connect(twitchHandler, SIGNAL(pDebug(QString,DebugLevel,QString)),
                this, SIGNAL(pDebug(QString,DebugLevel,QString)));
    }
}


void DraftHandler::deleteTwitchHandler()
{
    if(twitchHandler != nullptr)
    {
        delete twitchHandler;
        twitchHandler = nullptr;
    }
}


void DraftHandler::twitchHandlerConnectionOk(bool ok)
{
    if(ok)
    {
        if(draftScoreWindow != nullptr && TwitchHandler::isActive())   draftScoreWindow->showTwitchScores();
        if(draftHeroWindow != nullptr && TwitchHandler::isActive())    draftHeroWindow->showTwitchScores();
    }
    else
    {
        deleteTwitchHandler();
    }
}


void DraftHandler::twitchHandlerVoteUpdate(int vote1, int vote2, int vote3, QString username)
{
    if(draftScoreWindow != nullptr)    draftScoreWindow->setTwitchScores(vote1, vote2, vote3, username);
    if(draftHeroWindow != nullptr)     draftHeroWindow->setTwitchScores(vote1, vote2, vote3, username);
}


void DraftHandler::updateTwitchChatVotes()
{
    if(draftScoreWindow != nullptr)
    {
        if(twitchHandler != nullptr && twitchHandler->isConnectionOk() &&
                TwitchHandler::isActive())  draftScoreWindow->showTwitchScores();
        else                                draftScoreWindow->showTwitchScores(false);
    }

    if(draftHeroWindow != nullptr)
    {
        if(twitchHandler != nullptr && twitchHandler->isConnectionOk() &&
                TwitchHandler::isActive())  draftHeroWindow->showTwitchScores();
        else                                draftHeroWindow->showTwitchScores(false);
    }
}


void DraftHandler::initSynergyCounters(QList<DeckCard> &deckCardList)
{
    if(deckCardList.count() == 1 || synergyHandler->draftedCardsCount() > 0 || !patreonVersion)  return;

    if(!lavaButton->isEnabled())
    {
        scoreButtonHA->setEnabled(true);
        scoreButtonLF->setEnabled(true);
        scoreButtonHSR->setEnabled(true);
        lavaButton->setEnabled(true);
    }

    QMap<QString, QString> spellMap, minionMap, weaponMap,
                drop2Map, drop3Map, drop4Map,
                aoeMap, tauntMap, survivabilityMap, drawMap,
                pingMap, damageMap, destroyMap, reachMap;
    int tdraw, ttoYourHand, tdiscover;
    tdraw = ttoYourHand = tdiscover = 0;
    for(DeckCard &deckCard: deckCardList)
    {
        if(deckCard.getType() == INVALID_TYPE)  continue;
        QString code = deckCard.getCode();
        for(int i=0; i<deckCard.total; i++)
        {
            int draw, toYourHand, discover;
            synergyHandler->updateCounters(
                           deckCard,
                           spellMap, minionMap, weaponMap,
                           drop2Map, drop3Map, drop4Map,
                           aoeMap, tauntMap, survivabilityMap, drawMap,
                           pingMap, damageMap, destroyMap, reachMap,
                           draw, toYourHand, discover);
            tdraw += draw;
            ttoYourHand += toYourHand;
            tdiscover += discover;

            deckRatingHA += hearthArenaTiers[code];
            deckRatingLF += lightForgeTiers[code];
            deckRatingHSR += (cardsIncludedWinratesMap == nullptr) ? 0 : cardsIncludedWinratesMap[this->arenaHero][code];
        }
    }

    int numCards = synergyHandler->draftedCardsCount();
    lavaButton->setValue(synergyHandler->getManaCounterCount(), numCards, tdraw, ttoYourHand, tdiscover);

    updateDeckScore();
    emit pDebug("Counters starts with " + QString::number(numCards) + " cards.");
}


void DraftHandler::endDraft()
{
    if(!drafting)    return;

    emit pLog(tr("Draft: ") + ui->labelDeckScore->text());
    emit pDebug("End draft.");
    emit pLog(tr("Draft: Draft ended."));


    //SizeDraft
    QMainWindow *mainWindow = static_cast<QMainWindow*>(parent());
    QSettings settings("Arena Tracker", "Arena Tracker");
    settings.setValue("sizeDraft", mainWindow->size());

    //Hide Tab
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->tabDraft));
    ui->tabWidget->setCurrentIndex(ui->tabWidget->indexOf(ui->tabArena));
    emit calculateMinimumWidth();

    //SizePreDraft
    QSize size = settings.value("size", QSize(400, 400)).toSize();
    mainWindow->resize(size);

    //Upload or complete deck with assets
    //Set updateTime in log
    emit draftEnded();

    //Show Deck Score
    if(patreonVersion)
    {
        int numCards = synergyHandler->draftedCardsCount();
        int deckScoreHA = (numCards==0)?0:static_cast<int>(deckRatingHA/numCards);
        int deckScoreLF = (numCards==0)?0:static_cast<int>(deckRatingLF/numCards);
        float deckScoreHSR = (numCards==0)?0:static_cast<float>(round(static_cast<double>(deckRatingHSR/numCards * 10))/10.0);
        showMessageDeckScore(deckScoreLF, deckScoreHA, deckScoreHSR);
    }

    clearLists(false);

    this->drafting = false;
    this->justPickedCard = "";

    deleteDraftScoreWindow();
    deleteTwitchHandler();
}


void DraftHandler::heroDraftDeck(QString hero)
{
    CardClass newArenaHero = Utility::classLogNumber2classEnum(hero);//INVALID_CLASS if empty

    //Cierra mechanics si el heroe de la arena es diferente, permite cambiar de servidor
    if(draftMechanicsWindow != nullptr && this->arenaHero != newArenaHero)
    {
        emit pDebug("Delete draft mechanic window of different hero.", DebugLevel::Warning);
        deleteDraftMechanicsWindow();
    }

    this->arenaHero = newArenaHero;
}


//End game or end draft or enter previous arena (create Mechanics Window)
void DraftHandler::endDraftShowMechanicsWindow()
{
    if(drafting)
    {
        saveTemplateSettings();
        endDraft();
    }
    else if(draftMechanicsWindow != nullptr)    showOverlay();
    else
    {
        //Build mechanics window and init mechanics.
        buildDraftMechanicsWindow();
    }
}


//Start game
void DraftHandler::endDraftHideMechanicsWindow()
{
    stopLoops = true;

    if(drafting)            endDraft();
    else if(heroDrafting)   endHeroDraft();
    if(draftMechanicsWindow != nullptr)
    {
        draftMechanicsWindow->hide();

        //Cierra mechanics si esta incompleto asi se volvera a crear una vez la decklist este completa
        if(draftMechanicsWindow->draftedCardsCount() != 30)
        {
            emit pDebug("Delete draft mechanic window of incomplete deck.", DebugLevel::Warning);
            deleteDraftMechanicsWindow();
        }
    }
}


void DraftHandler::closeFindScreenRects()
{
    stopLoops = true;

    if(findingFrame && futureFindScreenRects.isRunning())   futureFindScreenRects.waitForFinished();
}


void DraftHandler::buildDraftMechanicsWindow()
{
    deleteDraftMechanicsWindow();

    QString hero = Utility::classEnum2classLogNumber(this->arenaHero);
    QList<DeckCard> *deckCardList = deckHandler->getDeckComplete();

    int heroInt = hero.toInt();
    if(deckCardList == nullptr)
    {
        emit pDebug("Build draft mechanic window of incomplete deck.", DebugLevel::Warning);
        return;
    }
    else if(heroInt<1 || heroInt>NUM_HEROS)
    {
        emit pDebug("Build draft mechanic window of unknown hero: " + hero, DebugLevel::Error);
        return;
    }
    else
    {
        emit pDebug("Build draft mechanic window. Heroe: " + hero);
    }

    clearLists(false);

    initCodesAndHistMaps(hero);
    initSynergyCounters(*deckCardList);
}


void DraftHandler::deleteDraftHeroWindow()
{
    if(draftHeroWindow != nullptr)
    {
        draftHeroWindow->close();
        delete draftHeroWindow;
        draftHeroWindow = nullptr;
        emit overlayCardLeave();
    }
}


void DraftHandler::deleteDraftScoreWindow()
{
    if(draftScoreWindow != nullptr)
    {
        draftScoreWindow->close();
        delete draftScoreWindow;
        draftScoreWindow = nullptr;
        emit overlayCardLeave();
    }
}


void DraftHandler::deleteDraftMechanicsWindow()
{
    if(draftMechanicsWindow != nullptr)
    {
        draftMechanicsWindow->close();
        delete draftMechanicsWindow;
        draftMechanicsWindow = nullptr;
        emit itemLeave();
    }
}


void DraftHandler::newCaptureDraftLoop(bool delayed)
{
    stopLoops = false;

    if(!capturing && screenFound() && cardsDownloading.isEmpty() &&
        ((drafting && !lightForgeTiers.empty() && !hearthArenaTiers.empty()) || heroDrafting))
    {
        capturing = true;

        if(delayed)                 QTimer::singleShot(CAPTUREDRAFT_START_TIME, this, SLOT(captureDraft()));
        else                        captureDraft();
    }
}


//Screen Rects detectados
void DraftHandler::captureDraft()
{
    justPickedCard = "";

    bool missingTierLists = drafting && (lightForgeTiers.empty() || hearthArenaTiers.empty());
    if((!drafting && !heroDrafting) || missingTierLists ||
        stopLoops || !screenFound() || !cardsDownloading.isEmpty())
    {
        capturing = false;
        return;
    }

    cv::MatND screenCardsHist[3];
    if(!getScreenCardsHist(screenCardsHist))
    {
        capturing = false;
        return;
    }
    mapBestMatchingCodes(screenCardsHist);

    if(areCardsDetected())
    {
        capturing = false;
        buildBestMatchesMaps();

        if(drafting)
        {
            DraftCard bestCards[3];
            getBestCards(bestCards);
            showNewCards(bestCards);
        }
        else if(heroDrafting)
        {
            showNewHeroes();
        }
    }
    else
    {
        if(numCaptured == 0)    QTimer::singleShot(CAPTUREDRAFT_LOOP_TIME_FADING, this, SLOT(captureDraft()));
        else                    QTimer::singleShot(CAPTUREDRAFT_LOOP_TIME, this, SLOT(captureDraft()));
    }
}


bool DraftHandler::areCardsDetected()
{
    for(int i=0; i<3; i++)
    {
        if(!cardDetected[i] && (numCaptured > 2) &&
            (getMinMatch(draftCardMaps[i]) < (CARD_ACCEPTED_THRESHOLD + numCaptured*CARD_ACCEPTED_THRESHOLD_INCREASE)))
        {
            cardDetected[i] = true;
        }
    }

    return (cardDetected[0] && cardDetected[1] && cardDetected[2]);
}


double DraftHandler::getMinMatch(const QMap<QString, DraftCard> &draftCardMaps)
{
    double minMatch = 1;
    for(DraftCard card: draftCardMaps.values())
    {
        double match = card.getBestQualityMatches();
        if(match < minMatch)    minMatch = match;
    }
    return minMatch;
}


void DraftHandler::buildBestMatchesMaps()
{
    if(drafting)
    {
        for(int i=0; i<3; i++)
        {
            QMap<double, QString> bestMatchesDups;
            for(QString code: draftCardMaps[i].keys())
            {
                double match = draftCardMaps[i][code].getBestQualityMatches();
                bestMatchesDups.insertMulti(match, code);
            }

            comboBoxCard[i]->clear();
            QStringList insertedCodes;
            for(const QString &code: bestMatchesDups.values())
            {
                if(!insertedCodes.contains(degoldCode(code)))
                {
                    double match = draftCardMaps[i][code].getBestQualityMatches();
                    bestMatchesMaps[i].insertMulti(match, code);
                    draftCardMaps[i][code].draw(comboBoxCard[i]);
                    insertedCodes.append(degoldCode(code));
                }
            }
        }
    }
    else if(heroDrafting)
    {
        for(int i=0; i<3; i++)
        {
            for(QString code: draftCardMaps[i].keys())
            {
                double match = draftCardMaps[i][code].getBestQualityMatches();
                bestMatchesMaps[i].insertMulti(match, code);
            }
        }
    }
}

//Distingue grupos de legendarias de no legendarias
//En los eventos las cartas legendarias introducidas no tienen rareza legendaria, para ellas no analizaremos rarezas
CardRarity DraftHandler::getBestRarity()
{
    CardRarity rarity[3];
    for(int i=0; i<3; i++)
    {
        if(bestMatchesMaps[i].isEmpty())    return INVALID_RARITY;
        QString code = bestMatchesMaps[i].first();
        //No restringimos rarezas si hay cartas unicas de arena (no colleccionables) (que no tienen rareza)
        if(!Utility::getCardAttribute(degoldCode(code), "collectible").toBool())    return INVALID_RARITY;
        rarity[i] = draftCardMaps[i][code].getRarity();
    }

//    if(rarity[0] == rarity[1] || rarity[0] == rarity[2])    return rarity[0];
//    else if(rarity[1] == rarity[2])                         return rarity[1];
//    else
    {
        double bestMatch = 1;
        int bestIndex = 0;

        for(int i=0; i<3; i++)
        {
            double match = bestMatchesMaps[i].firstKey();
            if(match < bestMatch)
            {
                bestMatch = match;
                bestIndex = i;
            }
        }

        return rarity[bestIndex];
    }
}


void DraftHandler::getBestCards(DraftCard bestCards[3])
{
    CardRarity bestRarity = getBestRarity();

    for(int i=0; i<3; i++)
    {
        QList<double> bestMatchesList = bestMatchesMaps[i].keys();
        QList<QString> bestCodesList = bestMatchesMaps[i].values();
        for(int j=0; j<bestMatchesList.count(); j++)
        {
            double match = bestMatchesList[j];
            QString code = bestCodesList[j];
            QString name = draftCardMaps[i][code].getName();
            QString cardInfo = code + " " + name + " " +
                    QString::number(static_cast<int>(match*1000)/1000.0);
            if( (bestRarity == INVALID_RARITY) ||
                (bestRarity != LEGENDARY && draftCardMaps[i][code].getRarity() != LEGENDARY) ||
                (bestRarity == LEGENDARY && draftCardMaps[i][code].getRarity() == LEGENDARY))
            {
                bestCards[i] = draftCardMaps[i][code];
                comboBoxCard[i]->setCurrentIndex(j);
                emit pDebug("Choose: " + cardInfo);
                break;
            }
            else
            {
                emit pDebug("Skip: " + cardInfo + " (Wrong rarity)");
            }
        }
        if(bestCards[i].getCode().isEmpty() && !bestCodesList.isEmpty())
        {
            bestCards[i] = draftCardMaps[i][bestCodesList.first()];
        }
    }

    connectAllComboBox();
    emit pDebug("(" + QString::number(synergyHandler->draftedCardsCount()) + ") " +
                bestCards[0].getCode() + "/" + bestCards[1].getCode() +
                "/" + bestCards[2].getCode() + " New codes.");
}


void DraftHandler::pickCard(QString code)
{
    if(!drafting || justPickedCard==code)
    {
        emit pDebug("WARNING: Duplicate pick code detected: " + code);
        return;
    }

    bool delayCapture = true;
    if(code=="0" || code=="1" || code=="2")
    {
        code = draftCards[code.toInt()].getCode();
        delayCapture = false;
    }

    //Avoid the pick of hero powers in dual class arena
    bool pickHeroPower = (Utility::getTypeFromCode(code) == HERO_POWER);

    if(patreonVersion && !pickHeroPower)
    {
        if(!lavaButton->isEnabled())
        {
            scoreButtonHA->setEnabled(true);
            scoreButtonLF->setEnabled(true);
            scoreButtonHSR->setEnabled(true);
            lavaButton->setEnabled(true);
        }

        DraftCard draftCard;
        int cardIndex;
        for(cardIndex=0; cardIndex<3; cardIndex++)
        {
            if(draftCards[cardIndex].getCode() == code)
            {
                draftCard = draftCards[cardIndex];
                break;
            }
        }

        if(cardIndex > 2)   draftCard = DraftCard(code);

        QMap<QString, QString> spellMap, minionMap, weaponMap,
                    drop2Map, drop3Map, drop4Map,
                    aoeMap, tauntMap, survivabilityMap, drawMap,
                    pingMap, damageMap, destroyMap, reachMap;
        int draw, toYourHand, discover;
        synergyHandler->updateCounters(draftCard,
                                       spellMap, minionMap, weaponMap,
                                       drop2Map, drop3Map, drop4Map,
                                       aoeMap, tauntMap, survivabilityMap, drawMap,
                                       pingMap, damageMap, destroyMap, reachMap,
                                       draw, toYourHand, discover);

        int numCards = synergyHandler->draftedCardsCount();
        lavaButton->setValue(synergyHandler->getManaCounterCount(), numCards, draw, toYourHand, discover);
        updateDeckScore(hearthArenaTiers[code], lightForgeTiers[code],
                        (cardsIncludedWinratesMap == nullptr) ? 0 : cardsIncludedWinratesMap[this->arenaHero][code]);
        if(draftMechanicsWindow != nullptr)
        {
            draftMechanicsWindow->updateCounters(spellMap, minionMap, weaponMap,
                                                 drop2Map, drop3Map, drop4Map,
                                                 aoeMap, tauntMap, survivabilityMap, drawMap,
                                                 pingMap, damageMap, destroyMap, reachMap,
                                                 synergyHandler->getCorrectedCardMana(draftCard), numCards);
            draftMechanicsWindow->updateDeckWeight(numCards, draw, toYourHand, discover);
        }
    }

    //Clear cards and score
    clearAndDisconnectAllComboBox();
    for(int i=0; i<3; i++)
    {
        clearScore(labelLFscore[i], LightForge);
        clearScore(labelHAscore[i], HearthArena);
        clearScore(labelHSRscore[i], HSReplay);
        draftCards[i].setCode("");
        draftCards[i].draw(comboBoxCard[i]);
        comboBoxCard[i]->setCurrentIndex(0);
        cardDetected[i] = false;
        draftCardMaps[i].clear();
        bestMatchesMaps[i].clear();
    }

    this->numCaptured = 0;
    this->extendedCapture = false;
    this->resetTwitchScores = true;
    if(draftScoreWindow != nullptr)    draftScoreWindow->hideScores();

    if(!pickHeroPower)
    {
        emit pDebug("Pick card: " + code);
        emit newDeckCard(code);
    }
    this->justPickedCard = code;

    newCaptureDraftLoop(delayCapture);
}


void DraftHandler::refreshCapturedCards()
{
    if(!drafting)   return;

    //Clear cards and score
    clearAndDisconnectAllComboBox();
    for(int i=0; i<3; i++)
    {
        clearScore(labelLFscore[i], LightForge);
        clearScore(labelHAscore[i], HearthArena);
        clearScore(labelHSRscore[i], HSReplay);
        draftCards[i].setCode("");
        draftCards[i].draw(comboBoxCard[i]);
        comboBoxCard[i]->setCurrentIndex(0);
        cardDetected[i] = false;
        draftCardMaps[i].clear();
        bestMatchesMaps[i].clear();
    }

    this->numCaptured = 0;
    this->extendedCapture = true;
    this->resetTwitchScores = false;
    if(draftScoreWindow != nullptr)    draftScoreWindow->hideScores();

    newCaptureDraftLoop();
}


void DraftHandler::refreshHeroes()
{
    for(int i=0; i<3; i++)
    {
        cardDetected[i] = false;
        draftCardMaps[i].clear();
        bestMatchesMaps[i].clear();
    }

    numCaptured = 0;
    if(draftHeroWindow != nullptr)  draftHeroWindow->hideScores();

    newCaptureDraftLoop();
}


void DraftHandler::showNewCards(DraftCard bestCards[3])
{
    //Load cards
    for(int i=0; i<3; i++)
    {
        clearScore(labelLFscore[i], LightForge);
        clearScore(labelHAscore[i], HearthArena);
        clearScore(labelHSRscore[i], HSReplay);
        draftCards[i] = bestCards[i];
    }

    QString codes[3] = {bestCards[0].getCode(), bestCards[1].getCode(), bestCards[2].getCode()};

    //LightForge
    int rating1 = lightForgeTiers[codes[0]];
    int rating2 = lightForgeTiers[codes[1]];
    int rating3 = lightForgeTiers[codes[2]];
    showNewRatings(rating1, rating2, rating3,
                   rating1, rating2, rating3,
                   LightForge);


    //HearthArena
    rating1 = hearthArenaTiers[codes[0]];
    rating2 = hearthArenaTiers[codes[1]];
    rating3 = hearthArenaTiers[codes[2]];
    showNewRatings(rating1, rating2, rating3,
                   rating1, rating2, rating3,
                   HearthArena);

    //HSReplay
    float ratingPlayed1, ratingPlayed2, ratingPlayed3;
    float ratingIncluded1, ratingIncluded2, ratingIncluded3;
    int includedDecks1, includedDecks2, includedDecks3;
    ratingPlayed1 = ratingPlayed2 = ratingPlayed3 = 0;
    ratingIncluded1 = ratingIncluded2 = ratingIncluded3 = 0;
    includedDecks1 = includedDecks2 = includedDecks3 = 0;

    if(cardsPlayedWinratesMap != nullptr)
    {
        ratingPlayed1 = cardsPlayedWinratesMap[this->arenaHero][codes[0]];
        ratingPlayed2 = cardsPlayedWinratesMap[this->arenaHero][codes[1]];
        ratingPlayed3 = cardsPlayedWinratesMap[this->arenaHero][codes[2]];
    }
    if(cardsIncludedWinratesMap != nullptr)
    {
        ratingIncluded1 = cardsIncludedWinratesMap[this->arenaHero][codes[0]];
        ratingIncluded2 = cardsIncludedWinratesMap[this->arenaHero][codes[1]];
        ratingIncluded3 = cardsIncludedWinratesMap[this->arenaHero][codes[2]];
    }
    if(cardsIncludedDecksMap != nullptr)
    {
        includedDecks1 = cardsIncludedDecksMap[this->arenaHero][codes[0]];
        includedDecks2 = cardsIncludedDecksMap[this->arenaHero][codes[1]];
        includedDecks3 = cardsIncludedDecksMap[this->arenaHero][codes[2]];
    }
    showNewRatings(ratingIncluded1, ratingIncluded2, ratingIncluded3,
                   ratingPlayed1, ratingPlayed2, ratingPlayed3,
                   HSReplay,
                   includedDecks1, includedDecks2, includedDecks3);

    //Twitch Handler
    if(this->twitchHandler != nullptr)
    {
        //No twitch reset al usar boton refresh o comboBox change.
        if(this->resetTwitchScores) twitchHandler->reset();

        if(TwitchHandler::isActive())
        {
            QString pickTag = TwitchHandler::getPickTag();
            twitchHandler->sendMessage((patreonVersion?QString("["+QString::number(synergyHandler->draftedCardsCount()+1)+"/30] -- "):QString("")) +
                                       "(" + pickTag + "1) " + bestCards[0].getName() +
                                       " / (" + pickTag + "2) " + bestCards[1].getName() +
                                       " / (" + pickTag + "3) " + bestCards[2].getName());
        }
    }


    if(patreonVersion)
    {
        if(draftScoreWindow != nullptr)
        {
            for(int i=0; i<3; i++)
            {
                QMap<QString, QMap<QString, int>> synergyTagMap;
                QMap<QString, int> mechanicIcons;
                MechanicBorderColor dropBorderColor;
                synergyHandler->getSynergies(bestCards[i], synergyTagMap, mechanicIcons, dropBorderColor);
                draftScoreWindow->setSynergies(i, synergyTagMap, mechanicIcons, dropBorderColor);
            }
        }
    }
}


void DraftHandler::comboBoxChanged()
{
    DraftCard bestCards[3];

    for(int i=0; i<3; i++)
    {
        int comboBoxIndex = comboBoxCard[i]->currentIndex();
        QList<QString> bestCodes = bestMatchesMaps[i].values();
        int count = bestCodes.count();
        if(comboBoxIndex >= count || comboBoxIndex < 0) return;
        QString code = bestCodes[comboBoxIndex];
        bestCards[i] = draftCardMaps[i][code];
    }

    if(draftScoreWindow != nullptr)    draftScoreWindow->hideScores(true);
    this->resetTwitchScores = false;
    showNewCards(bestCards);
}


void DraftHandler::updateDeckScore(float cardRatingHA, float cardRatingLF, float cardRatingHSR)
{
    if(!patreonVersion) return;

    int numCards = synergyHandler->draftedCardsCount();
    deckRatingHA += static_cast<int>(cardRatingHA);
    deckRatingLF += static_cast<int>(cardRatingLF);
    deckRatingHSR += cardRatingHSR;
    int deckScoreHA = (numCards==0)?0:static_cast<int>(deckRatingHA/numCards);
    int deckScoreLF = (numCards==0)?0:static_cast<int>(deckRatingLF/numCards);
    float deckScoreHSR = (numCards==0)?0:static_cast<float>(round(static_cast<double>(deckRatingHSR/numCards * 10))/10.0);
    updateLabelDeckScore(deckScoreLF, deckScoreHA, deckScoreHSR, numCards);
    scoreButtonLF->setScore(deckScoreLF, deckScoreLF);
    scoreButtonHA->setScore(deckScoreHA, deckScoreHA);
    scoreButtonHSR->setScore(deckScoreHSR, deckScoreHSR);

    if(draftMechanicsWindow != nullptr)    draftMechanicsWindow->setScores(deckScoreHA, deckScoreLF, deckScoreHSR);
}


QString DraftHandler::getDeckAvgString(int deckScoreLF, int deckScoreHA, float deckScoreHSR)
{
    QString scoreText = "";
    if(draftMethodLF)   scoreText += "LF: " + QString::number(deckScoreLF);
    if(draftMethodHSR)
    {
        if(!scoreText.isEmpty())    scoreText += " -- ";
        scoreText += "HSR: " + QString::number(static_cast<double>(deckScoreHSR)) + '%';
    }
    if(draftMethodHA)
    {
        if(!scoreText.isEmpty())    scoreText += " -- ";
        scoreText += "HA: " + QString::number(deckScoreHA);
    }
    return scoreText;
}


void DraftHandler::updateLabelDeckScore(int deckScoreLF, int deckScoreHA, float deckScoreHSR, int numCards)
{
    QString scoreText = getDeckAvgString(deckScoreLF, deckScoreHA, deckScoreHSR);
    scoreText += " (" + QString::number(numCards) + "/30)";
    ui->labelDeckScore->setText(scoreText);
}


void DraftHandler::showMessageDeckScore(int deckScoreLF, int deckScoreHA, float deckScoreHSR)
{
    QString scoreText = getDeckAvgString(deckScoreLF, deckScoreHA, deckScoreHSR);
    if(!scoreText.isEmpty())    emit showMessageProgressBar(scoreText, 10000);
}


void DraftHandler::showNewRatings(float rating1, float rating2, float rating3,
                                  float tierScore1, float tierScore2, float tierScore3,
                                  DraftMethod draftMethod,
                                  int includedDecks1, int includedDecks2, int includedDecks3)
{
    float ratings[3] = {rating1,rating2,rating3};
    float tierScore[3] = {tierScore1, tierScore2, tierScore3};
    float maxRating = std::max(std::max(rating1,rating2),rating3);
    int includedDecks[3] = {includedDecks1, includedDecks2, includedDecks3};

    for(int i=0; i<3; i++)
    {
        //Update score label
        if(draftMethod == LightForge)
        {
            labelLFscore[i]->setText(QString::number(static_cast<int>(ratings[i])));
            if(FLOATEQ(maxRating, ratings[i]))  highlightScore(labelLFscore[i], draftMethod);
        }
        else if(draftMethod == HSReplay)
        {
            QString text = QString::number(static_cast<double>(ratings[i])) + "% -- " +
                    QString::number(static_cast<double>(tierScore[i])) + "%";
            if(includedDecks[i] >= 0 && includedDecks[i] < MIN_HSR_DECKS)
            {
                text += " -- " + QString::number(includedDecks[i]) + " played";
            }

            labelHSRscore[i]->setText(text);
            if(FLOATEQ(maxRating, ratings[i]))  highlightScore(labelHSRscore[i], draftMethod);
        }
        else if(draftMethod == HearthArena)
        {
            labelHAscore[i]->setText(QString::number(static_cast<int>(ratings[i])));
            if(FLOATEQ(maxRating, ratings[i]))  highlightScore(labelHAscore[i], draftMethod);
        }
    }

    //Mostrar score
    if(draftScoreWindow != nullptr)
    {
        draftScoreWindow->setScores(rating1, rating2, rating3, draftMethod, includedDecks1, includedDecks2, includedDecks3);
    }
}


bool DraftHandler::getScreenCardsHist(cv::MatND screenCardsHist[3])
{
    QList<QScreen *> screens = QGuiApplication::screens();
    if(screenIndex >= screens.count() || screenIndex < 0)  return false;
    QScreen *screen = screens[screenIndex];
    if (!screen) return false;

    QRect rect = screen->geometry();
    QImage image = screen->grabWindow(0,rect.x(),rect.y(),rect.width(),rect.height()).toImage();
    cv::Mat mat(image.height(),image.width(),CV_8UC4,image.bits(), static_cast<ulong>(image.bytesPerLine()));

    cv::Mat screenCapture = mat.clone();

    cv::Mat bigCards[3];
    bigCards[0] = screenCapture(screenRects[0]);
    bigCards[1] = screenCapture(screenRects[1]);
    bigCards[2] = screenCapture(screenRects[2]);


//#ifdef QT_DEBUG
//    cv::imshow("Card1", bigCards[0]);
//    cv::imshow("Card2", bigCards[1]);
//    cv::imshow("Card3", bigCards[2]);
//#endif

    for(int i=0; i<3; i++)  screenCardsHist[i] = getHist(bigCards[i]);
    return true;
}


bool DraftHandler::isGoldCode(QString fileName)
{
    return fileName.endsWith("_premium");
}


QString DraftHandler::degoldCode(QString fileName)
{
    QString code = fileName;
    if(code.endsWith("_premium"))   code.chop(8);
    return code;
}


void DraftHandler::mapBestMatchingCodes(cv::MatND screenCardsHist[3])
{
    bool newCardsFound = false;
    const int numCandidates = (extendedCapture?CAPTURE_EXTENDED_CANDIDATES:CAPTURE_MIN_CANDIDATES);

    for(int i=0; i<3; i++)
    {
        QMap<double, QString> bestMatchesMap;
        for(QMap<QString, cv::MatND>::const_iterator it=cardsHist.constBegin(); it!=cardsHist.constEnd(); it++)
        {
            double match = compareHist(screenCardsHist[i], it.value(), 3);
            QString code = it.key();
            bestMatchesMap.insertMulti(match, code);

            //Actualizamos DraftCardMaps con los nuevos resultados
            if((numCaptured != 0) && draftCardMaps[i].contains(code))
            {
                draftCardMaps[i][code].setBestQualityMatch(match, false);
            }
        }

        //Incluimos en DraftCardMaps los mejores 7 matches, si no han sido ya actualizados por estar en el map.
        QList<double> bestMatchesList = bestMatchesMap.keys();
        for(int j=0; j<numCandidates && j<bestMatchesList.count(); j++)
        {
            double match = bestMatchesList.at(j);
            QString code = bestMatchesMap[match];

            if(!draftCardMaps[i].contains(code))
            {
                newCardsFound = true;
                draftCardMaps[i].insert(code, DraftCard(degoldCode(code)));
                if(numCaptured != 0)    draftCardMaps[i][code].setBestQualityMatch(match, true);
            }
        }
    }


    //No empezamos a contar mientras sigan apareciendo nuevas cartas en las 7 mejores posiciones
    if(numCaptured != 0 || !newCardsFound)
    {
        if(numCaptured == 0)
        {
            for(int i=0; i<3; i++)  draftCardMaps[i].clear();
        }

        this->numCaptured++;
    }


//#ifdef QT_DEBUG
//    for(int i=0; i<3; i++)
//    {
//        qDebug()<<endl;
//        for(QString code: draftCardMaps[i].keys())
//        {
//            DraftCard card = draftCardMaps[i][code];
//            qDebug()<<"["<<i<<"]"<<code<<card.getName()<<" -- "<<
//                      (static_cast<int>(card.getBestQualityMatches()*1000))/1000.0;
//        }
//    }
//    qDebug()<<"Captured: "<<numCaptured<<endl;
//#endif
}


cv::MatND DraftHandler::getHist(const QString &code)
{
    cv::Mat fullCard = cv::imread((Utility::hscardsPath() + "/" + code + ".png").toStdString(), CV_LOAD_IMAGE_COLOR);
    cv::Mat srcBase;
    if(drafting)
    {
        if(code.endsWith("_premium"))
        {
            if(fullCard.cols<(57+80) || fullCard.rows<(71+80))
            {
                emit pDebug("Card premium cv::Rect overflow.");
                cv::MatND emptyHist;
                return emptyHist;
            }
            srcBase = fullCard(cv::Rect(57,71,80,80));
        }
        else
        {
            if(fullCard.cols<(60+80) || fullCard.rows<(71+80))
            {
                emit pDebug("Card cv::Rect overflow.");
                cv::MatND emptyHist;
                return emptyHist;
            }
            srcBase = fullCard(cv::Rect(60,71,80,80));
        }
    }
    else if(heroDrafting)
    {
        if(fullCard.cols<(75+160) || fullCard.rows<(201+160))
        {
            emit pDebug("Hero cv::Rect overflow.");
            cv::MatND emptyHist;
            return emptyHist;
        }
        srcBase = fullCard(cv::Rect(75,201,160,160));
//#ifdef QT_DEBUG
//        cv::imshow(code.toStdString(), srcBase);
//#endif
    }
    return getHist(srcBase);
}


cv::MatND DraftHandler::getHist(cv::Mat &srcBase)
{
    cv::Mat hsvBase;

    /// Convert to HSV
    cvtColor( srcBase, hsvBase, cv::COLOR_BGR2HSV );

    /// Using 50 bins for hue and 60 for saturation
    int h_bins = 50; int s_bins = 60;
    int histSize[] = { h_bins, s_bins };

    // hue varies from 0 to 179, saturation from 0 to 255
    float h_ranges[] = { 0, 180 };
    float s_ranges[] = { 0, 256 };
    const float* ranges[] = { h_ranges, s_ranges };

    // Use the o-th and 1-st channels
    int channels[] = { 0, 1 };

    /// Calculate the histograms for the HSV images
    cv::MatND histBase;
    calcHist( &hsvBase, 1, channels, cv::Mat(), histBase, 2, histSize, ranges, true, false );
    normalize( histBase, histBase, 0, 1, cv::NORM_MINMAX, -1, cv::Mat() );

    return histBase;
}


bool DraftHandler::screenFound()
{
    if(screenIndex != -1)   return true;
    else                    return false;
}


bool DraftHandler::loadTemplateSettings()
{
    QSettings settings("Arena Tracker", "Arena Tracker");
    if(heroDrafting)
    {
        screenIndex = settings.value("heroDraftingScreenIndex", -1).toInt();
        if(screenIndex == -1)   return false;

        QRect rect;
        rect = settings.value("heroDraftingScreenRect0", QRect()).value<QRect>();
        screenRects[0]=cv::Rect(rect.x(), rect.y(), rect.width(), rect.height());
        rect = settings.value("heroDraftingScreenRect1", QRect()).value<QRect>();
        screenRects[1]=cv::Rect(rect.x(), rect.y(), rect.width(), rect.height());
        rect = settings.value("heroDraftingScreenRect2", QRect()).value<QRect>();
        screenRects[2]=cv::Rect(rect.x(), rect.y(), rect.width(), rect.height());

        screenScale = settings.value("heroDraftingScreenScale", QPointF(1,1)).value<QPointF>();
    }
    else// if(drafting) || buildMechanicsWindow
    {
        screenIndex = settings.value("draftingScreenIndex", -1).toInt();
        if(screenIndex == -1)   return false;

        QRect rect;
        rect = settings.value("draftingScreenRect0", QRect()).value<QRect>();
        screenRects[0]=cv::Rect(rect.x(), rect.y(), rect.width(), rect.height());
        rect = settings.value("draftingScreenRect1", QRect()).value<QRect>();
        screenRects[1]=cv::Rect(rect.x(), rect.y(), rect.width(), rect.height());
        rect = settings.value("draftingScreenRect2", QRect()).value<QRect>();
        screenRects[2]=cv::Rect(rect.x(), rect.y(), rect.width(), rect.height());

        screenScale = settings.value("draftingScreenScale", QPointF(1,1)).value<QPointF>();
    }
    return true;
}


bool DraftHandler::saveTemplateSettings()
{
    if(screenIndex == -1)   return false;

    QSettings settings("Arena Tracker", "Arena Tracker");
    if(heroDrafting)
    {
        settings.setValue("heroDraftingScreenIndex", screenIndex);

        QRect rect0(screenRects[0].x, screenRects[0].y, screenRects[0].width, screenRects[0].height);
        settings.setValue("heroDraftingScreenRect0", rect0);
        QRect rect1(screenRects[1].x, screenRects[1].y, screenRects[1].width, screenRects[1].height);
        settings.setValue("heroDraftingScreenRect1", rect1);
        QRect rect2(screenRects[2].x, screenRects[2].y, screenRects[2].width, screenRects[2].height);
        settings.setValue("heroDraftingScreenRect2", rect2);

        settings.setValue("heroDraftingScreenScale", screenScale);

        emit pDebug("Save HERO_DRAFT Screen Settings.");
    }
    else// if(drafting) //buildMechanicsWindow no guarda valores pq no hace startFindScreenRects();
    {
        settings.setValue("draftingScreenIndex", screenIndex);

        QRect rect0(screenRects[0].x, screenRects[0].y, screenRects[0].width, screenRects[0].height);
        settings.setValue("draftingScreenRect0", rect0);
        QRect rect1(screenRects[1].x, screenRects[1].y, screenRects[1].width, screenRects[1].height);
        settings.setValue("draftingScreenRect1", rect1);
        QRect rect2(screenRects[2].x, screenRects[2].y, screenRects[2].width, screenRects[2].height);
        settings.setValue("draftingScreenRect2", rect2);

        settings.setValue("draftingScreenScale", screenScale);

        emit pDebug("Save DRAFT Screen Settings.");
    }
    return true;
}


bool DraftHandler::isFindScreenOk(ScreenDetection &screenDetection)
{
    //Prueba de fallos
//        "[0]" 438 274 119 118
//        "[1]" 719 274 119 118
//        "[2]" 999 274 119 118
//        DRAFT -> 0.109259 BIEN / HERO DRAFT -> 0.146296 BIEN
//        "[0]" 421 344 159 158
//        "[1]" 702 344 159 158
//        "[2]" 982 344 159 158
//        DRAFT -> 0.146296 MAL
    float maxDistortion;
    if(heroDrafting)    maxDistortion = 0.156;
    else                maxDistortion = 0.119;// if(drafting) || buildMechanicsWindow
    for(int i=0; i<3; i++)
    {
        if(((screenDetection.screenRects[i].width/static_cast<float>(screenDetection.screenHeight)) > maxDistortion) ||
                ((screenDetection.screenRects[i].height/static_cast<float>(screenDetection.screenHeight)) > maxDistortion))
        {
            emit pDebug("WARNING: Hearthstone arena screen detected: Bad shape: "
                    "W(" + QString::number(screenDetection.screenRects[i].width) + "/" +
                    QString::number(screenDetection.screenHeight) +
                    "=" + QString::number(screenDetection.screenRects[i].width/static_cast<float>(screenDetection.screenHeight)) +
                    ") H(" + QString::number(screenDetection.screenRects[i].height) + "/" +
                    QString::number(screenDetection.screenHeight) +
                    "=" + QString::number(screenDetection.screenRects[i].height/static_cast<float>(screenDetection.screenHeight)) +
                    "). Retrying...");
            return false;
        }
    }
    return true;
}


bool DraftHandler::isFindScreenAsSettings(ScreenDetection &screenDetection)
{
    int maxDiff = screenRects[0].width/20;

    emit pDebug("Screen Settings: I(" + QString::number(screenIndex) + ")");
    for(int i=0; i<3; i++)
        emit pDebug("[" + QString::number(i) + "](" +
                QString::number(screenRects[i].x) + "," + QString::number(screenRects[i].y) + "," +
                QString::number(screenRects[i].width) + "," + QString::number(screenRects[i].height) + ")");
    emit pDebug("Screen Found: I(" + QString::number(screenDetection.screenIndex) + ")");
    for(int i=0; i<3; i++)
        emit pDebug("[" + QString::number(i) + "](" +
                QString::number(screenDetection.screenRects[i].x) + "," + QString::number(screenDetection.screenRects[i].y) + "," +
                QString::number(screenDetection.screenRects[i].width) + "," + QString::number(screenDetection.screenRects[i].height) + ")");

    if(screenIndex != screenDetection.screenIndex)  return false;
    for(int i=0; i<3; i++)
    {
        if(std::abs(screenRects[i].x - screenDetection.screenRects[i].x) > maxDiff)             return false;
        if(std::abs(screenRects[i].y - screenDetection.screenRects[i].y) > maxDiff)             return false;
        if(std::abs(screenRects[i].width - screenDetection.screenRects[i].width) > maxDiff)     return false;
        if(std::abs(screenRects[i].height - screenDetection.screenRects[i].height) > maxDiff)   return false;
    }
    return true;
}


void DraftHandler::newFindScreenLoop(bool skipScreenSettings)
{
    stopLoops = false;

    //skipScreenSettings = Force Draft
    if(!skipScreenSettings && loadTemplateSettings())
    {
        emit pDebug("Hearthstone arena screen loaded from settings.");
        createDraftWindows();
        if(drafting || heroDrafting)
        {
            newCaptureDraftLoop();
            if(draftMechanicsWindow != nullptr) draftMechanicsWindow->hide();
        }
        else    return;
    }
    else
    {
        emit pDebug("Hearthstone arena screen NOT loaded from settings.");
    }

    if(!findingFrame && (drafting || heroDrafting))//buildMechanicsWindow no busca frame
    {
        findingFrame = true;
        startFindScreenRects();
    }
}


void DraftHandler::startFindScreenRects()
{
    if(stopLoops)
    {
        findingFrame = false;
        return;
    }
    if(!futureFindScreenRects.isRunning())
    {
        futureFindScreenRects.setFuture(QtConcurrent::run(this, &DraftHandler::findScreenRects));
    }
}


void DraftHandler::finishFindScreenRects()
{
    ScreenDetection screenDetection = futureFindScreenRects.result();

    if(stopLoops)
    {
        findingFrame = false;
        return;
    }

    if(screenDetection.screenIndex == -1)
    {
        emit pDebug("Hearthstone arena screen not found. Retrying...");
        QTimer::singleShot(FINDINGFRAME_LOOP_TIME, this, SLOT(startFindScreenRects()));
    }
    else if(isFindScreenOk(screenDetection))
    {
        bool isSame = isFindScreenAsSettings(screenDetection);
        bool needCreate = (screenIndex == -1);
        findingFrame = false;
        this->screenIndex = screenDetection.screenIndex;
        this->screenScale = screenDetection.screenScale;
        for(int i=0; i<3; i++)  this->screenRects[i] = screenDetection.screenRects[i];
        emit pDebug("Hearthstone arena screen detected on screen " + QString::number(screenIndex) +
                    ". " + (isSame?QString("It's"):QString("Not")) + " the same.");

        if(needCreate)
        {
            createDraftWindows();
            if(drafting || heroDrafting)    newCaptureDraftLoop();
        }
        else
        {
            if(isSame)  showOverlay();
            else
            {
                createDraftWindows();

                if(drafting)            refreshCapturedCards();
                else if(heroDrafting)   refreshHeroes();
            }
        }
    }
    else    QTimer::singleShot(FINDINGFRAME_LOOP_TIME, this, SLOT(startFindScreenRects()));
}


ScreenDetection DraftHandler::findScreenRects()
{
    ScreenDetection screenDetection;

    std::vector<Point2f> templatePoints(6);
    if(heroDrafting)
    {
        templatePoints[0] = cvPoint(182,332); templatePoints[1] = cvPoint(182+152,332+152);
        templatePoints[2] = cvPoint(453,332); templatePoints[3] = cvPoint(453+152,332+152);
        templatePoints[4] = cvPoint(724,332); templatePoints[5] = cvPoint(724+152,332+152);
    }
    else// if(drafting) || buildMechanicsWindow
    {
        templatePoints[0] = cvPoint(205,276); templatePoints[1] = cvPoint(205+118,276+118);
        templatePoints[2] = cvPoint(484,276); templatePoints[3] = cvPoint(484+118,276+118);
        templatePoints[4] = cvPoint(762,276); templatePoints[5] = cvPoint(762+118,276+118);
    }


    QList<QScreen *> screens = QGuiApplication::screens();
    for(int screenIndex=0; screenIndex<screens.count(); screenIndex++)
    {
        QScreen *screen = screens[screenIndex];
        if (!screen)    continue;

        QString arenaTemplate;
        if(drafting)    arenaTemplate = "arenaTemplate.png";
        else if(heroDrafting)   arenaTemplate = "heroesTemplate.png";
        else                    arenaTemplate = "mechanicsTemplate.png";
        std::vector<Point2f> screenPoints = Utility::findTemplateOnScreen(arenaTemplate, screen, templatePoints,
                                                screenDetection.screenScale, screenDetection.screenHeight);
        if(screenPoints.empty())    continue;

        //Calculamos screenRect
        for(int i=0; i<3; i++)
        {
            screenDetection.screenRects[i]=cv::Rect(screenPoints[static_cast<ulong>(i*2)], screenPoints[static_cast<ulong>(i*2+1)]);
        }

        screenDetection.screenIndex = screenIndex;
        return screenDetection;
    }

    screenDetection.screenIndex = -1;
    return screenDetection;
}


void DraftHandler::beginHeroDraft()
{
    emit pDebug("Begin hero draft.");

    deleteDraftMechanicsWindow();
    clearLists(false);
    this->heroDrafting = true;

    initCodesAndHistMaps();
    createTwitchHandler();
}


void DraftHandler::endHeroDraft()
{
    if(!heroDrafting)    return;

    emit pDebug("End hero draft.");

    clearLists(false);

    this->heroDrafting = false;
    deleteDraftHeroWindow();
    deleteTwitchHandler();
}


void DraftHandler::showNewHeroes()
{
    float scores[3];
    int classOrder[3];
    for(int i=0; i<3; i++)
    {
        double match = bestMatchesMaps[i].firstKey();
        QString code = bestMatchesMaps[i].first();
        QString name = draftCardMaps[i][code].getName();
        QString cardInfo = code + " " + name + " " +
                QString::number(static_cast<int>(match*1000)/1000.0);
        emit pDebug("Choose: " + cardInfo);

        QString HSRkey = Utility::getCardAttribute(code, "cardClass").toString();
        scores[i] = heroWinratesMap[HSRkey];
        classOrder[i] = Utility::className2classOrder(HSRkey);
    }
    if(draftHeroWindow != nullptr)     draftHeroWindow->setScores(scores, classOrder);

    //Twitch Handler
    if(this->twitchHandler != nullptr)
    {
        twitchHandler->reset();

        if(TwitchHandler::isActive())
        {
            QString pickTag = TwitchHandler::getPickTag();
            twitchHandler->sendMessage("(" + pickTag + "1) " + draftCardMaps[0][bestMatchesMaps[0].first()].getName() +
                                       " / (" + pickTag + "2) " + draftCardMaps[1][bestMatchesMaps[1].first()].getName() +
                                       " / (" + pickTag + "3) " + draftCardMaps[2][bestMatchesMaps[2].first()].getName());
        }
    }
}


void DraftHandler::initDraftMechanicsWindowCounters()
{
    int numCards = synergyHandler->draftedCardsCount();

    if(numCards == 0 || !patreonVersion || draftMechanicsWindow == nullptr)    return;

    QMap<QString, QString> spellMap, minionMap, weaponMap,
                drop2Map, drop3Map, drop4Map,
                aoeMap, tauntMap, survivabilityMap, drawMap,
                pingMap, damageMap, destroyMap, reachMap;
    int draw, toYourHand, discover;
    int manaCounter = synergyHandler->getCounters(spellMap, minionMap, weaponMap,
                                                  drop2Map, drop3Map, drop4Map,
                                                  aoeMap, tauntMap, survivabilityMap, drawMap,
                                                  pingMap, damageMap, destroyMap, reachMap,
                                                  draw, toYourHand, discover);
    draftMechanicsWindow->updateCounters(spellMap, minionMap, weaponMap,
                                         drop2Map, drop3Map, drop4Map,
                                         aoeMap, tauntMap, survivabilityMap, drawMap,
                                         pingMap, damageMap, destroyMap, reachMap,
                                         manaCounter, numCards);
    draftMechanicsWindow->updateDeckWeight(numCards, draw, toYourHand, discover);
    updateDeckScore();
}


void DraftHandler::createDraftWindows()
{
    deleteDraftHeroWindow();
    deleteDraftScoreWindow();
    deleteDraftMechanicsWindow();
    QPoint topLeft(static_cast<int>(screenRects[0].x * screenScale.x()), static_cast<int>(screenRects[0].y * screenScale.y()));
    QPoint bottomRight(static_cast<int>(screenRects[2].x * screenScale.x() + screenRects[2].width * screenScale.x()),
            static_cast<int>(screenRects[2].y * screenScale.y() + screenRects[2].height * screenScale.y()));
    QRect draftRect(topLeft, bottomRight);
    QSize sizeCard(static_cast<int>(screenRects[0].width * screenScale.x()), static_cast<int>(screenRects[0].height * screenScale.y()));

    if(drafting)
    {
        emit pDebug("Create drafting windows.");
        draftScoreWindow = new DraftScoreWindow(static_cast<QMainWindow *>(this->parent()), draftRect, sizeCard, screenIndex);

        connect(draftScoreWindow, SIGNAL(cardEntered(QString,QRect,int,int)),
                this, SIGNAL(overlayCardEntered(QString,QRect,int,int)));
        connect(draftScoreWindow, SIGNAL(cardLeave()),
                this, SIGNAL(overlayCardLeave()));
        connect(draftScoreWindow, SIGNAL(showHSRwebPicks()),
                this, SLOT(showHSRwebPicks()));
        connect(draftScoreWindow, SIGNAL(pLog(QString)),
                this, SIGNAL(pLog(QString)));
        connect(draftScoreWindow, SIGNAL(pDebug(QString,DebugLevel,QString)),
                this, SIGNAL(pDebug(QString,DebugLevel,QString)));

        draftScoreWindow->setLearningMode(this->learningMode);
        draftScoreWindow->setDraftMethod(this->draftMethodHA, this->draftMethodLF, this->draftMethodHSR);
        if(twitchHandler != nullptr && twitchHandler->isConnectionOk() && TwitchHandler::isActive())   draftScoreWindow->showTwitchScores();

        draftMechanicsWindow = new DraftMechanicsWindow(static_cast<QMainWindow *>(this->parent()), draftRect, sizeCard, screenIndex,
                                                        patreonVersion);
        draftMechanicsWindow->setDraftMethodAvgScore(draftMethodAvgScore);
        draftMechanicsWindow->setShowDrops(this->showDrops);
        initDraftMechanicsWindowCounters();

        connect(draftMechanicsWindow, SIGNAL(itemEnter(QList<SynergyCard>&,QPoint&,int,int)),
                this, SIGNAL(itemEnterOverlay(QList<SynergyCard>&,QPoint&,int,int)));
        connect(draftMechanicsWindow, SIGNAL(itemLeave()),
                this, SIGNAL(itemLeave()));
        connect(draftMechanicsWindow, SIGNAL(showPremiumDialog()),
                this, SIGNAL(showPremiumDialog()));
    }
    else if(heroDrafting)
    {
        emit pDebug("Create heroDrafting windows.");
        draftHeroWindow = new DraftHeroWindow(static_cast<QMainWindow *>(this->parent()), draftRect, sizeCard, screenIndex);
        if(twitchHandler != nullptr && twitchHandler->isConnectionOk() && TwitchHandler::isActive())   draftHeroWindow->showTwitchScores();
    }
    else//buildMechanicsWindow
    {
        emit pDebug("Create mechanic window.");
        draftMechanicsWindow = new DraftMechanicsWindow(static_cast<QMainWindow *>(this->parent()), draftRect, sizeCard, screenIndex,
                                                        patreonVersion);
        draftMechanicsWindow->setDraftMethodAvgScore(draftMethodAvgScore);
        draftMechanicsWindow->setShowDrops(this->showDrops);
        initDraftMechanicsWindowCounters();
        //Despues de calcular todo podemos limpiar draftHandler, lo que nos interesa esta todo en draftMechanicsWindow
        clearLists(false);

        connect(draftMechanicsWindow, SIGNAL(itemEnter(QList<SynergyCard>&,QPoint&,int,int)),
                this, SIGNAL(itemEnterOverlay(QList<SynergyCard>&,QPoint&,int,int)));
        connect(draftMechanicsWindow, SIGNAL(itemLeave()),
                this, SIGNAL(itemLeave()));
        connect(draftMechanicsWindow, SIGNAL(showPremiumDialog()),
                this, SIGNAL(showPremiumDialog()));
    }

    showOverlay();
}


void DraftHandler::showHSRwebPicks()
{
    QString url = "https://hsreplay.net/cards/#playerClass=";
    url += Utility::classEnum2classUName(this->arenaHero);
    url += "&gameType=ARENA&text=";
    url += draftCards[0].getName() + ',' + draftCards[1].getName() + ',' + draftCards[2].getName();

    QDesktopServices::openUrl(QUrl(url));
}


void DraftHandler::clearScore(QLabel *label, DraftMethod draftMethod, bool clearText)
{
    if(clearText)   label->setText("");
    else if(label->styleSheet().contains("background-image"))
    {
        highlightScore(label, draftMethod);
        return;
    }

    if(!mouseInApp && transparency == Transparent)
    {
        label->setStyleSheet("QLabel {background-color: transparent; color: white;}");
    }
    else
    {
        label->setStyleSheet("");
    }
}


void DraftHandler::highlightScore(QLabel *label, DraftMethod draftMethod)
{
    QString backgroundImage = "";
    if(draftMethod == LightForge)           backgroundImage = ":/Images/bgScoreLF.png";
    else if(draftMethod == HearthArena)     backgroundImage = ":/Images/bgScoreHA.png";
    else if(draftMethod == HSReplay)        backgroundImage = ":/Images/bgScoreHSR.png";
    label->setStyleSheet("QLabel {background-color: transparent; color: " +
                         QString((!mouseInApp && transparency == Transparent)?"white":ThemeHandler::fgColor()) + ";"
                         "background-image: url(" + backgroundImage + "); background-repeat: no-repeat; background-position: center; }");
}


void DraftHandler::setTheme()
{
    if(draftMechanicsWindow != nullptr)    draftMechanicsWindow->setTheme();
    synergyHandler->setTheme();

    ui->refreshDraftButton->setIcon(QIcon(ThemeHandler::buttonDraftRefreshFile()));

    QFont font(ThemeHandler::bigFont());
    font.setPixelSize(24);
    ui->labelLFscore1->setFont(font);
    ui->labelLFscore2->setFont(font);
    ui->labelLFscore3->setFont(font);
    ui->labelHAscore1->setFont(font);
    ui->labelHAscore2->setFont(font);
    ui->labelHAscore3->setFont(font);
    ui->labelHSRscore1->setFont(font);
    ui->labelHSRscore2->setFont(font);
    ui->labelHSRscore3->setFont(font);

    for(int i=0; i<3; i++)
    {
        if(labelLFscore[i]->styleSheet().contains("background-image"))      highlightScore(labelLFscore[i], LightForge);
        if(labelHAscore[i]->styleSheet().contains("background-image"))      highlightScore(labelHAscore[i], HearthArena);
        if(labelHSRscore[i]->styleSheet().contains("background-image"))     highlightScore(labelHSRscore[i], HSReplay);
    }

    //Change Arena draft icon
    int index = ui->tabWidget->indexOf(ui->tabDraft);
    if(index >= 0)  ui->tabWidget->setTabIcon(index, QIcon(ThemeHandler::tabArenaFile()));
}


void DraftHandler::setTransparency(Transparency value)
{
    this->transparency = value;

    if(!mouseInApp && transparency==Transparent)
    {
        ui->tabDraft->setAttribute(Qt::WA_NoBackground);
        ui->tabDraft->repaint();

        ui->labelDeckScore->setStyleSheet("QLabel {background-color: transparent; color: white;}");
    }
    else
    {
        ui->tabDraft->setAttribute(Qt::WA_NoBackground, false);
        ui->tabDraft->repaint();

        ui->labelDeckScore->setStyleSheet("");
    }

    //Update score labels
    clearScore(ui->labelLFscore1, LightForge, false);
    clearScore(ui->labelLFscore2, LightForge, false);
    clearScore(ui->labelLFscore3, LightForge, false);
    clearScore(ui->labelHAscore1, HearthArena, false);
    clearScore(ui->labelHAscore2, HearthArena, false);
    clearScore(ui->labelHAscore3, HearthArena, false);
    clearScore(ui->labelHSRscore1, HSReplay, false);
    clearScore(ui->labelHSRscore2, HSReplay, false);
    clearScore(ui->labelHSRscore3, HSReplay, false);

    //Update race counters
    synergyHandler->setTransparency(transparency, mouseInApp);
}


void DraftHandler::setMouseInApp(bool value)
{
    this->mouseInApp = value;
    setTransparency(this->transparency);
}


void DraftHandler::setShowDraftScoresOverlay(bool value)
{
    this->showDraftScoresOverlay = value;
    showOverlay();
}


void DraftHandler::setShowDraftMechanicsOverlay(bool value)
{
    this->showDraftMechanicsOverlay = value;
    showOverlay();
}


void DraftHandler::showOverlay()
{
    if(this->draftHeroWindow != nullptr)
    {
        this->draftHeroWindow->show();
    }
    if(this->draftScoreWindow != nullptr)
    {
        if(showDraftScoresOverlay)  this->draftScoreWindow->show();
        else                        this->draftScoreWindow->hide();
    }

    if(this->draftMechanicsWindow != nullptr)
    {
        if(showDraftMechanicsOverlay && patreonVersion) this->draftMechanicsWindow->show();
        else                                            this->draftMechanicsWindow->hide();
    }
}


void DraftHandler::setLearningMode(bool value)
{
    this->learningMode = value;
    if(this->draftScoreWindow != nullptr)   draftScoreWindow->setLearningMode(value);

    updateScoresVisibility();
}


void DraftHandler::setShowDrops(bool value)
{
    this->showDrops = value;
    if(this->draftMechanicsWindow != nullptr)   draftMechanicsWindow->setShowDrops(value);
}


void DraftHandler::setDraftMethodAvgScore(DraftMethod draftMethodAvgScore)
{
    this->draftMethodAvgScore = draftMethodAvgScore;

    if(!isDrafting())   return;
    if(draftMechanicsWindow != nullptr)    draftMechanicsWindow->setDraftMethodAvgScore(draftMethodAvgScore);

    updateAvgScoresVisibility();
}


void DraftHandler::setDraftMethod(bool draftMethodHA, bool draftMethodLF, bool draftMethodHSR)
{
    this->draftMethodHA = draftMethodHA;
    this->draftMethodLF = draftMethodLF;
    this->draftMethodHSR = draftMethodHSR;

    if(!isDrafting())   return;
    if(draftScoreWindow != nullptr)        draftScoreWindow->setDraftMethod(draftMethodHA, draftMethodLF, draftMethodHSR);

    updateDeckScore();//Basicamente para updateLabelDeckScore
    updateScoresVisibility();
}


void DraftHandler::updateScoresVisibility()
{
    if(learningMode)
    {
        for(int i=0; i<3; i++)
        {
            labelLFscore[i]->hide();
            labelHAscore[i]->hide();
            labelHSRscore[i]->hide();
        }
    }
    else
    {
        for(int i=0; i<3; i++)
        {
            labelLFscore[i]->setVisible(draftMethodLF);
            labelHAscore[i]->setVisible(draftMethodHA);
            labelHSRscore[i]->setVisible(draftMethodHSR);
        }
    }
}


void DraftHandler::updateMinimumHeight()
{
    ui->tabDraft->setMinimumHeight(ui->tabDraft->sizeHint().height());
}


void DraftHandler::updateAvgScoresVisibility()
{
    scoreButtonLF->hide();
    scoreButtonHA->hide();
    scoreButtonHSR->hide();

    if(patreonVersion)
    {
        switch(draftMethodAvgScore)
        {
            case LightForge:
                scoreButtonLF->show();
            break;
            case HearthArena:
                scoreButtonHA->show();
            break;
            case HSReplay:
                scoreButtonHSR->show();
            break;
            default:
            break;
        }
    }
}


void DraftHandler::redrawAllCards()
{
    if(!drafting)   return;

    for(int i=0; i<3; i++)
    {
        int currentIndex = comboBoxCard[i]->currentIndex();
        clearAndDisconnectComboBox(i);
        for(const QString &code: bestMatchesMaps[i].values())
        {
            draftCardMaps[i][code].draw(comboBoxCard[i]);
        }
        comboBoxCard[i]->setCurrentIndex(currentIndex);
    }

    if(draftScoreWindow != nullptr)    draftScoreWindow->redrawSynergyCards();
    connectAllComboBox();
}


void DraftHandler::updateTamCard()
{
    ui->comboBoxCard1->setIconSize(QSize(DeckCard::getCardWidth(), DeckCard::getCardHeight()));
    ui->comboBoxCard2->setIconSize(QSize(DeckCard::getCardWidth(), DeckCard::getCardHeight()));
    ui->comboBoxCard3->setIconSize(QSize(DeckCard::getCardWidth(), DeckCard::getCardHeight()));
}


void DraftHandler::craftGoldenCopy(int cardIndex)
{
    QString code = draftCards[cardIndex].getCode();
    if(!drafting || code.isEmpty())  return;

    //Lanza script
    QProcess p;
    QStringList params;

    params << QDir::toNativeSeparators(Utility::extraPath() + "/goldenCrafter.py");
    params << Utility::removeAccents(draftCards[cardIndex].getName());//Card Name

    qDebug()<<"Start script:\n" + params.join(" - ");

#ifdef Q_OS_WIN
    p.start("python", params);
#else
    p.start("python3", params);
#endif
    p.waitForFinished(-1);
}


bool DraftHandler::isDrafting()
{
    return this->drafting;
}


void DraftHandler::minimizeScoreWindow()
{
    if(this->draftHeroWindow != nullptr)                                                       draftHeroWindow->showMinimized();
    if(this->draftScoreWindow != nullptr && showDraftScoresOverlay)                            draftScoreWindow->showMinimized();
    if(this->draftMechanicsWindow != nullptr && showDraftMechanicsOverlay && patreonVersion)   draftMechanicsWindow->showMinimized();
}


void DraftHandler::deMinimizeScoreWindow()
{
    if(this->draftHeroWindow != nullptr)                                                       draftHeroWindow->setWindowState(Qt::WindowActive);
    if(this->draftScoreWindow != nullptr && showDraftScoresOverlay)                            draftScoreWindow->setWindowState(Qt::WindowActive);
    if(this->draftMechanicsWindow != nullptr && showDraftMechanicsOverlay && patreonVersion)   draftMechanicsWindow->setWindowState(Qt::WindowActive);
}


void DraftHandler::setHeroWinratesMap(QMap<QString, float> &heroWinratesMap)
{
    this->heroWinratesMap = heroWinratesMap;
}


void DraftHandler::setCardsIncludedWinratesMap(QMap<QString, float> cardsIncludedWinratesMap[])
{
    this->cardsIncludedWinratesMap = cardsIncludedWinratesMap;
}


void DraftHandler::setCardsIncludedDecksMap(QMap<QString, int> cardsIncludedDecksMap[])
{
    this->cardsIncludedDecksMap = cardsIncludedDecksMap;
}


void DraftHandler::setCardsPlayedWinratesMap(QMap<QString, float> cardsPlayedWinratesMap[])
{
    this->cardsPlayedWinratesMap = cardsPlayedWinratesMap;
}


void DraftHandler::buildHeroCodesList()
{
    heroCodesList.clear();

    //--------------------------------------------------------
    //----NEW HERO CLASS
    //--------------------------------------------------------
    for(const QString &code: Utility::getSetCodes("CORE", false, true))
    {
        if(code.startsWith("HERO_0") || code.startsWith("HERO_1"))   heroCodesList.append(code);
    }
    for(const QString &code: Utility::getSetCodes("HERO_SKINS", false, true))
    {
        if(code.startsWith("HERO_0") || code.startsWith("HERO_1"))   heroCodesList.append(code);
    }
//    qDebug()<<endl<<"HERO CODES!!!!!!!!!!!!!!!!!!!!!!!!"<<endl<<heroCodesList<<endl;
}


//Construir json de HearthArena (Ya no lo usamos)
//1) Copiar line (var cards = ...)
//EL RESTO LO HACE EL SCRIPT
//2) Eliminar al principio ("\")
//3) Eliminar al final (\"";)
//4) Eliminar (\\\\\\\") Problemas con " en descripciones.
//(Ancien Spirit - Chaman)
//(Explorer's Hat - Hunter)
//(Soul of the forest - Druid)
//5) Eliminar todas las (\)

//Heroes
//01) Warrior
//02) Shaman
//03) Rogue
//04) Paladin
//05) Hunter
//06) Druid
//07) Warlock
//08) Mage
//09) Priest
