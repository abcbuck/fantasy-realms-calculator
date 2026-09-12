#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <tuple>
//#include <unordered_set>
using namespace std;

filesystem::path basePath;

int16_t best10[10*8] = {}; // 8 = 7 cards + 1 value
int16_t worst10[10*8] = {};

int16_t best10ForEach[10*8*52] = {};

const uint8_t _K = 7;
const uint8_t _N = 52; // ohne den Totenbeschwörer
// Den Totenbeschwörer sollte ich noch einmal mit einbeziehen, wenn alles funktioniert, weil er für das Juwel der Ordnung und den Sammler wichtig sein kann, und in diesen Fällen einen großen Unterschied macht.
// Idee: Den Totenbeschwörer zu jeder Hand hinzufügen, die eine Karte enthält, die am Ende durch ihn aufgenommen worden sein könnte.

typedef string name_t, colour_t;

const uint8_t COLOUR_COUNT = 10;
colour_t colours[COLOUR_COUNT] = {
  "Anführer",
  "Armee",
  "Artefakt",
  "Bestie",
  "Flamme",
  "Flut",
  "Land",
  "Waffe",
  "Wetter",
  "Zauberer"
};

struct Card;

struct Effects {
  // string ruleText;
  bool hasMultipleCombinations = false;
  uint8_t combinationCount = 1;
  bool invalid = false; // skip a combination that contains this card; used to simplify calculations where one card has alternative effects to choose from
  bool blanked = false;
  void (*specialEffect)(Card* hand, uint8_t index); // this always refers to a joker, the book of changes, unpunishing or an invalidating effect; nothing else

  void (*blank)(Card* hand, uint8_t index);
  int16_t (*punish)(Card* hand, uint8_t index);
  bool punishArmies = true; // indicates this card may punish army cards; if set to false, this punishment may be deleted
  // reworking the following variables into (*punish)()
  /* unordered_set<colour_t> blockColours; // block all given colours,
  unordered_set<colour_t> blockColourExceptions; // except for some colours
  unordered_set<name_t> blockNameExceptions; // or names
  colour_t selfBlockWith;
  colour_t selfBlockWithout;
  int punishValue = 0;
  unordered_set<colour_t> punishColours; // give a point punishment for every card of the given color
  name_t punishException;
  colour_t punishForNo; // give a punishment in points if the given color isn't present */
  //
  
  int16_t (*bonusPoints)(Card* hand, uint8_t index);

  /* Effects() { // The constructor may already be implicitly defined
    specialEffect = NULL;
    unblockColours = unordered_set<colour_t>();
    blockColours = unordered_set<colour_t>();
    unblockColourExceptions = unordered_set<colour_t>();
    blockNameExceptions = unordered_set<name_t>();
    selfBlockWith = "";
    selfBlockWithout = "";
    punishColours = unordered_set<colour_t>();
    punishForNo = "";
    bonusPoints = NULL;
  } */
  
  //outdated
  /* bool hasPunishment() {
    return !blockColours.empty() ||
      selfBlockWith != "" ||
      selfBlockWithout != "" ||
      !punishColours.empty() ||
      punishForNo != "";
  }

  void removePunishments() {
    blockColours.clear();
    selfBlockWith = "";
    selfBlockWithout = "";
    punishColours.clear();
    punishForNo = "";
  }

  void removeFromPunishments(colour_t colour) {
    blockColours.erase(colour);
    if(selfBlockWith == colour)
      selfBlockWith = "";
    if(selfBlockWithout == colour)
      selfBlockWithout = "";
    punishColours.erase(colour);
    if(punishForNo == colour)
      punishForNo = "";
  } */
};

struct Card {
  uint8_t index; // is used for execution order of the effects; therefore unpunishing cards should come first, right after cards that change some cards' colour or name
  // better use const char[size] once I know the maximum sizes
  name_t name;
  colour_t colour;
  uint8_t baseValue;
  Effects effects;
};

// all cards
Card cards[_N] = {};
void initializeCards() {
  for(uint8_t i=0; i<_N; i++)
    cards[i].index = i;

  cards[0].name = "Doppelgänger"; // Sammler references Doppelganger as having index 0! Be sure to change this reference, should you ever change the doppelganger index.
  cards[0].colour = "Joker";
  cards[0].baseValue = 0;
  cards[0].effects.hasMultipleCombinations = true;
  cards[0].effects.combinationCount = _K-1;
  cards[0].effects.specialEffect = [](Card* hand, uint8_t index) {
    uint8_t targetIndex = hand[index].effects.combinationCount;
    if(targetIndex >= index)
      targetIndex++;
    // copy name, base value, colour
    hand[index].name      = hand[targetIndex].name;
    hand[index].baseValue = hand[targetIndex].baseValue;
    hand[index].colour    = hand[targetIndex].colour;
    // and punishment
    hand[index].effects.blank        = hand[targetIndex].effects.blank;
    hand[index].effects.punish       = hand[targetIndex].effects.punish;
    hand[index].effects.punishArmies = hand[targetIndex].effects.punishArmies;
    /* hand[index].effects.blockColours          = hand[targetIndex].effects.blockColours;
    hand[index].effects.blockColourExceptions = hand[targetIndex].effects.blockColourExceptions;
    hand[index].effects.blockNameExceptions   = hand[targetIndex].effects.blockNameExceptions;
    hand[index].effects.selfBlockWith         = hand[targetIndex].effects.selfBlockWith;
    hand[index].effects.selfBlockWithout      = hand[targetIndex].effects.selfBlockWithout;
    hand[index].effects.punishValue           = hand[targetIndex].effects.punishValue;
    hand[index].effects.punishColours         = hand[targetIndex].effects.punishColours;
    hand[index].effects.punishException       = hand[targetIndex].effects.punishException;
    hand[index].effects.punishForNo           = hand[targetIndex].effects.punishForNo; */
  };

  cards[1].name = "Spiegelung";
  cards[1].colour = "Joker";
  cards[1].baseValue = 0;
  cards[1].effects.hasMultipleCombinations = true;
  cards[1].effects.combinationCount = (_N-3)/2+2;
  cards[1].effects.specialEffect = [](Card* hand, uint8_t index) {
    if(hand[index].effects.combinationCount == (_N-3)/2+1)
      return; //don't use the card effect
    //unordered_set<colour_t> validColours = {"Armee", "Land", "Wetter", "Flut", "Flamme"};
    const char* validColours[] = {"Armee", "Land", "Wetter", "Flut", "Flamme"};
    uint8_t count = 0;
    uint8_t target = 0;
    while(count != hand[index].effects.combinationCount || find(validColours, validColours+5, cards[target].colour) == validColours+5) {
      if(find(validColours, validColours+5, cards[target].colour) != validColours+5) // the colour we're looking for
        count++;
      target++;
    }
    // select card
    hand[index].name = cards[target].name;
    hand[index].colour = cards[target].colour;
  };

  cards[2].name = "Gestaltwandler";
  cards[2].colour = "Joker";
  cards[2].baseValue = 0;
  cards[2].effects.hasMultipleCombinations = true;
  cards[2].effects.combinationCount = (_N-3)/2+1;
  cards[2].effects.specialEffect = [](Card* hand, uint8_t index) {
    if(hand[index].effects.combinationCount == (_N-3)/2)
      return; //don't use the card effect
    //unordered_set<colour_t> validColours = {"Artefakt", "Anführer", "Zauberer", "Waffe", "Bestie"};
    const char* validColours[] = {"Artefakt", "Anführer", "Zauberer", "Waffe", "Bestie"};
    uint8_t count = 0;
    uint8_t target = 0;
    while(count != hand[index].effects.combinationCount || find(validColours, validColours+5, cards[target].colour) == validColours+5) {
      if(find(validColours, validColours+5, cards[target].colour) != validColours+5) // the colour we're looking for
        count++;
      target++;
    }
    // select card
    hand[index].name = cards[target].name;
    hand[index].colour = cards[target].colour;
  };

  cards[3].name = "Buch der Veränderung";
  cards[3].colour = "Artefakt";
  cards[3].baseValue = 3;
  cards[3].effects.hasMultipleCombinations = true;
  cards[3].effects.combinationCount = _K*COLOUR_COUNT; //6 other cards, 10 colours, 6*10 = 60; this value changes to each of the counts in every possible combination with the other cards if the card is included
  cards[3].effects.specialEffect = [](Card* hand, uint8_t index) {
    if(hand[index].effects.combinationCount / COLOUR_COUNT == index || hand[hand[index].effects.combinationCount / COLOUR_COUNT].colour == colours[hand[index].effects.combinationCount % COLOUR_COUNT]) {
      hand[index].effects.invalid = true;
      return;
    }

    hand[hand[index].effects.combinationCount / COLOUR_COUNT].colour = colours[hand[index].effects.combinationCount % COLOUR_COUNT];
  };

  cards[4].name = "Rune des Schutzes";
  cards[4].colour = "Artefakt";
  cards[4].baseValue = 1;
  cards[4].effects.specialEffect = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++) {
      hand[i].effects.blank = NULL;
      hand[i].effects.punish = NULL;
    }
  };

  cards[5].name = "Weltenbaum";
  cards[5].colour = "Artefakt";
  cards[5].baseValue = 2;
  cards[5].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    /* unordered_set<colour_t> coloursInHand;
    uint8_t unblankedCount = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        unblankedCount++;
        coloursInHand.insert(hand[i].colour);
      }
    return coloursInHand.size() == unblankedCount ? 50 : 0; */
    // seems simpler to me
    for(uint8_t i=0; i<_K-1; i++) {
      if(hand[i].effects.blanked)
        continue;
      for(uint8_t j=i+1; j<_K; j++) {
        if(hand[j].effects.blanked)
          continue;
        if(hand[i].colour == hand[j].colour)
          return 0;
      }
    }
    return 50;
  };

  cards[6].name = "Schild von Keth";
  cards[6].colour = "Artefakt";
  cards[6].baseValue = 4;
  cards[6].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    bool leader = false;
    bool sword = false;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        if(hand[i].colour == "Anführer")
          leader = true;
        if(hand[i].name == "Schwert von Keth")
          sword = true;
      }
    return leader ? (sword ? 40 : 15) : 0;
  };

  cards[7].name = "Juwel der Ordnung";
  cards[7].colour = "Artefakt";
  cards[7].baseValue = 5;
  cards[7].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t cardValues[_K];
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].effects.blanked)
        cardValues[i] = -100;
      else
        cardValues[i] = hand[i].baseValue;
    sort(cardValues, cardValues+_K);

    uint8_t straight = 1, maxStraight = 1;
    for(uint8_t i=1; i<_K; i++) {
      if(cardValues[i] == cardValues[i-1]+1)
        straight++;
      else {
        if(straight > maxStraight)
          maxStraight = straight;
        straight = 1;
      }
    }
    if(straight > maxStraight)
      maxStraight = straight;

    return 5*(maxStraight-2)*(maxStraight-1);
  };

  cards[8].name = "Magierin";
  cards[8].colour = "Zauberer";
  cards[8].baseValue = 5;
  cards[8].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    //unordered_set<colour_t> validColours = {"Land", "Wetter", "Flut", "Flamme"};
    const char* validColours[] = {"Land", "Wetter", "Flut", "Flamme"};
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && find(validColours, validColours+4, hand[i].colour) != validColours+4)
        sum += 5;
    return sum;
  };

  cards[9].name = "Sammler";
  cards[9].colour = "Zauberer";
  cards[9].baseValue = 7;
  cards[9].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t maxCount = 0;

    // get max count of same colour cards in hand
    int8_t prev = -1;
    uint8_t differentIndex = 0;
    while(differentIndex != prev) {
      prev = differentIndex;
      uint8_t count = 0;
      while(hand[differentIndex].effects.blanked || hand[differentIndex].index == 0) { // exclude doppelganger
        prev++;
        differentIndex++;
      }
      for(uint8_t i=differentIndex; i<_K; i++) {
        if(!hand[i].effects.blanked && hand[i].index != 0) { // exclude doppelganger
          if(hand[i].colour == hand[prev].colour)
            count++;
          else if(differentIndex == prev)
            differentIndex = i;
        }
      }
      if(count > maxCount)
        maxCount = count;
    }

    if(maxCount <= 2)
      return 0;
    if(maxCount == 3)
      return 10;
    if(maxCount == 4)
      return 40;
    if(maxCount >= 5)
      return 100;
    //return 0; // to make the compiler happy
  };

  cards[10].name = "Herr der Bestien";
  cards[10].colour = "Zauberer";
  cards[10].baseValue = 9;
  cards[10].effects.specialEffect = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].colour == "Bestie") {
        hand[i].effects.blank = NULL;
        hand[i].effects.punish = NULL;
      }
  };
  cards[10].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Bestie")
        sum += 9;
    return sum;
  };

  cards[11].name = "Hexenmeister";
  cards[11].colour = "Zauberer";
  cards[11].baseValue = 25;
  cards[11].effects.punish = [](Card* hand, uint8_t index) -> int16_t {
    int8_t punishment = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && (hand[i].colour == "Anführer" || hand[i].colour == "Zauberer") && i != index)
        punishment -= 10;
    return punishment;
  };

  cards[12].name = "Luftwesen";
  cards[12].colour = "Wetter";
  cards[12].baseValue = 4;
  cards[12].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && i != index && hand[i].colour == "Wetter")
        sum += 15;
    return sum;
  };

  cards[13].name = "Regensturm";
  cards[13].colour = "Wetter";
  cards[13].baseValue = 8;
  cards[13].effects.blank = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].colour == "Flamme" && hand[i].name != "Blitz")
        hand[i].effects.blanked = true;
  };
  cards[13].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Flut")
        sum += 10;
    return sum;
  };

  cards[14].name = "Wirbelsturm";
  cards[14].colour = "Wetter";
  cards[14].baseValue = 13;
  cards[14].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    bool rainStorm = false;
    bool blizzard = false;
    bool greatFlood = false;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        if(hand[i].name == "Regensturm")
          rainStorm = true;
        else if(hand[i].name == "Blizzard")
          blizzard = true;
        else if(hand[i].name == "Große Flut")
          greatFlood = true;
      }
    return rainStorm && (blizzard || greatFlood) ? 40 : 0;
  };

  cards[15].name = "Rauch";
  cards[15].colour = "Wetter";
  cards[15].baseValue = 27;
  cards[15].effects.blank = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].colour == "Flamme")
        return;
    hand[index].effects.blanked = true;
  };

  cards[16].name = "Blizzard";
  cards[16].colour = "Wetter";
  cards[16].baseValue = 30;
  cards[16].effects.blank = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].colour == "Flut")
        hand[i].effects.blanked = true;
  };
  cards[16].effects.punish = [](Card* hand, uint8_t index) -> int16_t {
    const char* condition[] = {"Armee", "Anführer", "Bestie", "Flamme"};
    int8_t punishment = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && find(condition, condition+4, hand[i].colour) != condition+4)
        punishment -= 5;
    return punishment;
  };

  cards[17].name = "Quelle des Lebens";
  cards[17].colour = "Flut";
  cards[17].baseValue = 1;
  cards[17].effects.hasMultipleCombinations = true;
  cards[17].effects.combinationCount = _K-1;
  cards[17].effects.specialEffect = [](Card* hand, uint8_t index) {
      uint8_t targetIndex = hand[index].effects.combinationCount;
      if(targetIndex >= index)
        targetIndex++;
      //unordered_set<colour_t> validColours = {"Waffe", "Flut", "Flamme", "Land", "Wetter"};
      const char* validColours[] = {"Waffe", "Flut", "Flamme", "Land", "Wetter"};
      //if(!validColours.contains(hand[targetIndex].colour))
      if(find(validColours, validColours+5, hand[targetIndex].colour) == validColours+5)
        hand[index].effects.invalid = true;
  };
  cards[17].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t targetIndex = hand[index].effects.combinationCount;
    if(targetIndex >= index)
      targetIndex++;
    // no need to check, already checked in specialEffect()
    //unordered_set<colour_t> validColours = {"Waffe", "Flut", "Flamme", "Land", "Wetter"};
    //if(validColours.contains(hand[targetIndex].colour))
    if(!hand[targetIndex].effects.blanked)
      return hand[targetIndex].baseValue;
    return 0;
  };

  cards[18].name = "Wasserwesen";
  cards[18].colour = "Flut";
  cards[18].baseValue = 4;
  cards[18].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && i != index && hand[i].colour == "Flut")
        sum += 15;
    return sum;
  };

  cards[19].name = "Insel";
  cards[19].colour = "Flut";
  cards[19].baseValue = 14;
  cards[19].effects.hasMultipleCombinations = true;
  cards[19].effects.combinationCount = 4; // there are at most three cards that fulfill this card's condition,
  // also consider holding this card without using its effect as a fourth option
  cards[19].effects.specialEffect = [](Card* hand, uint8_t index) {
    uint8_t candidateIndex = hand[index].effects.combinationCount;
    if(candidateIndex == 3)
      return;
    uint8_t candidateCount = 0;
    for(uint8_t i=0; i<_K; i++) {
      if((hand[i].colour == "Flut" || hand[i].colour == "Flamme") && (hand[i].effects.blank != NULL || hand[i].effects.punish != NULL)) {
        if(candidateCount == candidateIndex) {
          hand[i].effects.blank = NULL;
          hand[i].effects.punish = NULL;
          return;
        }
        candidateCount++;
      }
    }
    hand[index].effects.invalid = true;
  };

  cards[20].name = "Sumpf";
  cards[20].colour = "Flut";
  cards[20].baseValue = 18;
  cards[20].effects.punish = [](Card* hand, uint8_t index) -> int16_t {
    int8_t punishment = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && (hand[index].effects.punishArmies && hand[i].colour == "Armee" || hand[i].colour == "Flamme"))
        punishment -= 3;
    return punishment;
  };

  cards[21].name = "Große Flut";
  cards[21].colour = "Flut";
  cards[21].baseValue = 32;
  cards[21].effects.blank = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[index].effects.punishArmies && hand[i].colour == "Armee" ||
          hand[i].colour == "Land" && hand[i].name != "Gebirge" ||
          hand[i].colour == "Flamme" && hand[i].name != "Blitz")
        hand[i].effects.blanked = true;
  };

  cards[22].name = "Kerze";
  cards[22].colour = "Flamme";
  cards[22].baseValue = 2;
  cards[22].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    bool book = false;
    bool tower = false;
    bool wizard = false;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        if(hand[i].name == "Buch der Veränderung")
          book = true;
        else if(hand[i].name == "Glockenturm")
          tower = true;
        if(hand[i].colour == "Zauberer")
          wizard = true;
      }
    return book && tower && wizard ? 100 : 0;
  };

  cards[23].name = "Feuerwesen";
  cards[23].colour = "Flamme";
  cards[23].baseValue = 4;
  cards[23].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && i != index && hand[i].colour == "Flamme")
        sum += 15;
    return sum;
  };

  cards[24].name = "Schmiede";
  cards[24].colour = "Flamme";
  cards[24].baseValue = 9;
  cards[24].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && (hand[i].colour == "Waffe" || hand[i].colour == "Artefakt"))
        sum += 9;
    return sum;
  };

  cards[25].name = "Blitz";
  cards[25].colour = "Flamme";
  cards[25].baseValue = 11;
  cards[25].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].name == "Regensturm")
        return 30;
    return 0;
  };

  cards[26].name = "Buschfeuer";
  cards[26].colour = "Flamme";
  cards[26].baseValue = 40;
  cards[26].effects.blank = [](Card* hand, uint8_t index) {
    if(hand[index].effects.blanked)
      return;
    const char* blockColourExceptions[] = {"Flamme", "Zauberer", "Wetter", "Waffe", "Artefakt"};
    const char* blockNameExceptions[] = {"Gebirge", "Große Flut", "Insel", "Einhorn", "Drache"};
    for(uint8_t i=0; i<_K; i++)
      if(find(blockColourExceptions, blockColourExceptions+5, hand[i].colour) == blockColourExceptions+5 &&
          find(blockNameExceptions, blockNameExceptions+5, hand[i].name) == blockNameExceptions+5)
        hand[i].effects.blanked = true;
  };

  cards[27].name = "Schlachtross";
  cards[27].colour = "Bestie";
  cards[27].baseValue = 6;
  cards[27].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && (hand[i].colour == "Anführer" || hand[i].colour == "Zauberer"))
        return 14;
    return 0;
  };

  cards[28].name = "Einhorn";
  cards[28].colour = "Bestie";
  cards[28].baseValue = 9;
  cards[28].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    const char* condition[] = {"Kaiserin", "Königin", "Magierin"};
    uint8_t value = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        if(hand[i].name == "Prinzessin")
          return 30;
        else if(find(condition, condition+3, hand[i].name) != condition+3)
          value = 15;
      }
    return value;
  };

  cards[29].name = "Hydra";
  cards[29].colour = "Bestie";
  cards[29].baseValue = 12;
  cards[29].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].name == "Sumpf")
        return 28;
    return 0;
  };

  cards[30].name = "Drache";
  cards[30].colour = "Bestie";
  cards[30].baseValue = 30;
  cards[30].effects.punish = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Zauberer")
        return 0;
    return -40;
  };

  cards[31].name = "Basilisk";
  cards[31].colour = "Bestie";
  cards[31].baseValue = 35;
  cards[31].effects.blank = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[index].effects.punishArmies && hand[i].colour == "Armee" ||
          hand[i].colour == "Anführer" ||
          i != index && hand[i].colour == "Bestie")
        hand[i].effects.blanked = true;
  };

  cards[32].name = "Zauberstab";
  cards[32].colour = "Waffe";
  cards[32].baseValue = 1;
  cards[32].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Zauberer")
        return 25;
    return 0;
  };

  cards[33].name = "Elbischer Bogen";
  cards[33].colour = "Waffe";
  cards[33].baseValue = 3;
  cards[33].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    const char* condition[] = {"Elbenschützen", "Kriegsherr", "Herr der Bestien"};
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && find(condition, condition+3, hand[i].name) != condition+3)
        return 30;
    return 0;
  };

  cards[34].name = "Schwert von Keth";
  cards[34].colour = "Waffe";
  cards[34].baseValue = 7;
  cards[34].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    bool leader = false;
    bool shield = false;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        if(hand[i].colour == "Anführer")
          leader = true;
        if(hand[i].name == "Schild von Keth")
          shield = true;
      }
    return leader ? (shield ? 40 : 10) : 0;
  };

  cards[35].name = "Kriegsschiff";
  cards[35].colour = "Waffe";
  cards[35].baseValue = 23;
  cards[35].effects.specialEffect = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].colour == "Flut")
        hand[i].effects.punishArmies = false;
  };
  cards[35].effects.blank = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].colour == "Flut")
        return;
    hand[index].effects.blanked = true;
  };

  cards[36].name = "Kampfzeppelin";
  cards[36].colour = "Waffe";
  cards[36].baseValue = 35;
  cards[36].effects.blank = [](Card* hand, uint8_t index) {
    bool armyInHand = false;
    for(uint8_t i=0; i<_K; i++) {
      if(hand[i].colour == "Armee")
        armyInHand = true;
      else if(hand[i].colour == "Wetter") {
        hand[index].effects.blanked = true;
        return;
      }
    }
    if(hand[index].effects.punishArmies && !armyInHand)
      hand[index].effects.blanked = true;
  };

  cards[37].name = "Prinzessin";
  cards[37].colour = "Anführer";
  cards[37].baseValue = 2;
  cards[37].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    const char* validColours[] = {"Armee", "Zauberer", "Anführer"};
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && i != index && find(validColours, validColours+3, hand[i].colour) != validColours+3)
        sum += 8;
    return sum;
  };

  cards[38].name = "Kriegsherr";
  cards[38].colour = "Anführer";
  cards[38].baseValue = 4;
  cards[38].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Armee")
        sum += hand[i].baseValue;
    return sum;
  };

  cards[39].name = "Königin";
  cards[39].colour = "Anführer";
  cards[39].baseValue = 6;
  cards[39].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    bool king = false;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        if(hand[i].colour == "Armee")
          sum += 5;
        if(hand[i].name == "König")
          king = true;
      }
    if(king)
      sum *= 4;
    return sum;
  };

  cards[40].name = "König";
  cards[40].colour = "Anführer";
  cards[40].baseValue = 8;
  cards[40].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    bool queen = false;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        if(hand[i].colour == "Armee")
          sum += 5;
        if(hand[i].name == "Königin")
          queen = true;
      }
    if(queen)
      sum *= 4;
    return sum;
  };

  cards[41].name = "Kaiserin";
  cards[41].colour = "Anführer";
  cards[41].baseValue = 15;
  cards[41].effects.punish = [](Card* hand, uint8_t index) -> int16_t {
    int8_t punishment = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && i != index && hand[i].colour == "Anführer")
        punishment -= 5;
    return punishment;
  };

  cards[42].name = "Erdwesen";
  cards[42].colour = "Land";
  cards[42].baseValue = 4;
  cards[42].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && i != index && hand[i].colour == "Land")
        sum += 15;
    return sum;
  };

  cards[43].name = "Höhle";
  cards[43].colour = "Land";
  cards[43].baseValue = 6;
  cards[43].effects.specialEffect = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].colour == "Wetter") {
        hand[i].effects.blank = NULL;
        hand[i].effects.punish = NULL;
      }
  };
  cards[43].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && (hand[i].name == "Zwergeninfanterie" || hand[i].name == "Drache"))
        return 25;
    return 0;
  };

  cards[44].name = "Wald";
  cards[44].colour = "Land";
  cards[44].baseValue = 7;
  cards[44].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && (hand[i].colour == "Bestie" || hand[i].name == "Elbenschützen"))
        sum += 12;
    return sum;
  };

  cards[45].name = "Glockenturm";
  cards[45].colour = "Land";
  cards[45].baseValue = 8;
  cards[45].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Zauberer")
        return 15;
    return 0;
  };

  cards[46].name = "Gebirge";
  cards[46].colour = "Land";
  cards[46].baseValue = 9;
  cards[46].effects.specialEffect = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].colour == "Flut") {
        hand[i].effects.blank = NULL;
        hand[i].effects.punish = NULL;
      }
  };
  cards[46].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    bool smoke = false;
    bool wildfire = false;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        if(hand[i].name == "Rauch")
          smoke = true;
        else if(hand[i].name == "Buschfeuer") {
          wildfire = true;
        }
      }
    return smoke && wildfire ? 50 : 0;
  };

  cards[47].name = "Waldläufer";
  cards[47].colour = "Armee";
  cards[47].baseValue = 5;
  cards[47].effects.specialEffect = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      hand[i].effects.punishArmies = false;
  };
  cards[47].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Land")
        sum += 10;
    return sum;
  };

  cards[48].name = "Elbenschützen";
  cards[48].colour = "Armee";
  cards[48].baseValue = 10;
  cards[48].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Wetter")
        return 0;
    return 5;
  };

  cards[49].name = "Zwergeninfanterie";
  cards[49].colour = "Armee";
  cards[49].baseValue = 15;
  cards[49].effects.punish = [](Card* hand, uint8_t index) -> int16_t {
    int8_t punishment = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && i != index && hand[i].colour == "Armee")
        punishment -= 2;
    return punishment;
  };

  cards[50].name = "Leichte Kavallerie";
  cards[50].colour = "Armee";
  cards[50].baseValue = 17;
  cards[50].effects.punish = [](Card* hand, uint8_t index) -> int16_t {
    int8_t punishment = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Land")
        punishment -= 2;
    return punishment;
  };

  cards[51].name = "Ritter";
  cards[51].colour = "Armee";
  cards[51].baseValue = 20;
  cards[51].effects.punish = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Anführer")
        return 0;
    return -8;
  };
}

struct Combination {
  Card cards[_K];
  int16_t value;
};

/* //vector<Combination> combinations;
const int BUFFER_SIZE = 1048580; // a multiple of 13; 1 MiB + 4 Bytes
char buffer[BUFFER_SIZE];
int bufferIndex = 0;

void writeToFile() {
  fstream f;
  f.open("fantasy_realms_combinations.data", ios_base::app | ios_base::out | ios_base::binary);
  f.write(buffer, BUFFER_SIZE);
  f.close();
} */


/*
def combinations(k, n=0): # indices of combinations of k elements of a list of length n;
    # if n is set to 0, the list is treated as having infinitely many elements
    c = list(range(k))
    yield c
    i = 0
    while True:
        if i == k-1 or c[i]+1 != c[i+1]:
            c[i] += 1
            if c[i] == n:
                break
            i = 0
            yield c
        else:
            c[i] = i
            i += 1
*/
void forCombinationsDo(void (*hand_fn)(uint8_t*), uint8_t* startCombination) {
  uint8_t combination[_K];
  for(uint8_t i=0; i<_K; i++)
    combination[i] = startCombination[i];
  //combination[_K-1] = 26; //delete
  hand_fn(combination);
  uint8_t i = 0;
  while(true) {
    if(i==_K-1 || combination[i]+1 != combination[i+1]) {
      combination[i]++;
      if(combination[i] == _N)
        break;
      i=0;
      hand_fn(combination);
    } else {
      combination[i] = i;
      i++;
    }
  }
}

bool nextSelection(uint8_t* selection, uint8_t* limits) {
  uint8_t i=0;
  while(true) {
    selection[i]++;
    if(selection[i] == limits[i])
      selection[i++] = 0;
    else
      return true;
    if(i == _K)
      return false;
  }
}
/*
  calculate the value of the combination
  store the combination + value in an ordered list, ordered by total value descending
  store the list to memory
  get the 10 best combinations + value
  get the 10 best combinations for every card + value
*/
unsigned long combinationCounter = 0;
void calculate(uint8_t* combination) { // todo: Doppelnennungen derselben Kombination mit demselben Punktestand (nur mit unterschiedlicher Effektverwendung) nicht speichern
  // calculate the value of the combination
  Card hand[_K];
  uint8_t selectionLimits[_K];
  for(uint8_t i=0; i<_K; i++) {
    hand[i] = cards[combination[i]]; // copy card
    // add multiple combinations for relevant cards!
    selectionLimits[i] = hand[i].effects.combinationCount;
  }
  //iterate through the cartesian product of card options
  uint8_t selection[_K] = {};
  for(bool isValidSelection = true; isValidSelection; isValidSelection = nextSelection(selection, selectionLimits)) {
    //reset cards for every selection
    for(uint8_t i=0; i<_K; i++) {
      hand[i] = cards[combination[i]]; // copy card
    }
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].effects.hasMultipleCombinations)
        hand[i].effects.combinationCount = selection[i];
    
    /* // debug
    if(hand[0].index == 0 &&
       hand[1].index == 9 &&
       hand[2].index == 12 &&
       hand[3].index == 13 &&
       hand[4].index == 14 &&
       hand[5].index == 15 &&
       hand[6].index == 16)
       println("Debug"); */

    // apply effects and calculate
    int16_t totalValue = 0;
    {
      // do each of the following for every hand card before continuing to the next step

      // special effects (includes unblocking)
      // and skip combinations with invalid=true
      bool invalid = false;
      for(uint8_t i=0; i<_K; i++)
        if(hand[i].effects.specialEffect != NULL) {
          hand[i].effects.specialEffect(hand, i);
          if(hand[i].effects.invalid) {
            invalid = true;
            break;
          }
        }
      if(invalid)
        continue;

      // punish

      // blank
      // first determine the blank order: start with unblanked cards
      {
        for(uint8_t i=0; i<_K; i++)
          if(hand[i].effects.blank != NULL)
            hand[i].effects.blank(hand, i);
        uint8_t unblankeds[7] = {};
        uint8_t unblankedsSize = 0; // number of unblankeds in the array 'unblankeds'; after the next loop the remaining indices refer to blanked cards, either by unblanked cards or by one another
        uint8_t blankedIndex = 6;
        for(uint8_t i=0; i<_K; i++) 
          if(!hand[i].effects.blanked)
            unblankeds[unblankedsSize++] = i;
          else {
            hand[i].effects.blanked = false;
            unblankeds[blankedIndex--] = i;
          }
        {
          uint8_t i=0;
          // apply blank effect of unblanked cards
          for(; i<unblankedsSize; i++)
            if(hand[unblankeds[i]].effects.blank != NULL)
              hand[unblankeds[i]].effects.blank(hand, unblankeds[i]);

          // for the remaining cards, check which cards are still unblanked and apply all their effects even if they blank each other
          unblankedsSize = 0;
          for(; i<_K; i++) 
            if(!hand[unblankeds[i]].effects.blanked)
              unblankeds[unblankedsSize++] = unblankeds[i];

        }
        for(uint8_t i=0; i<unblankedsSize; i++)
          if(hand[unblankeds[i]].effects.blank != NULL)
            hand[unblankeds[i]].effects.blank(hand, unblankeds[i]);
      }

        /* // blank other cards
        if(!hand[i].effects.blockColours.empty())
          for(int j=0; j<_K; j++)
            if(hand[i].effects.blockColours.contains(hand[j].colour) && // block all given colours,
            !hand[i].effects.blockColourExceptions.contains(hand[j].colour) && // except for some colours
            !hand[i].effects.blockNameExceptions.contains(hand[j].name)) // or names
              hand[j].effects.blanked = true;

        // self-blank
        if(!hand[i].effects.blanked && hand[i].effects.selfBlockWith != "")
          for(int j=0; j<_K; j++)
            if(hand[j].colour == hand[i].effects.selfBlockWith) {
              hand[i].effects.blanked = true;
              break;
            }
        if(!hand[i].effects.blanked && hand[i].effects.selfBlockWithout != "") {
          bool block = true;
          for(int j=0; j<_K; j++)
            if(hand[j].colour == hand[i].effects.selfBlockWithout) {
              block = false;
              break;
            }
          if(block)
            hand[i].effects.blanked = true;
        } */
      

      // punish value (if not blanked)
      for(uint8_t i=0; i<_K; i++)
        if(!hand[i].effects.blanked && hand[i].effects.punish != NULL)
          totalValue += hand[i].effects.punish(hand, i);

      // bonus (for non-blanked cards)
      for(uint8_t i=0; i<_K; i++)
        if(!hand[i].effects.blanked) {
          totalValue += hand[i].baseValue;
          if(hand[i].effects.bonusPoints != NULL)
            totalValue += hand[i].effects.bonusPoints(hand, i);
        }
    }

    // store the combination + value in an ordered list, ordered by total value descending
    /* Combination c;
    copy(hand, hand+_K, c.cards);
    c.value = totalValue; */

    /* println("{} points: {} ({}), {} ({}), {} ({}), {} ({}), {} ({}), {} ({}), {} ({})", totalValue,
      c.cards[0].name, c.cards[0].index,
      c.cards[1].name, c.cards[1].index,
      c.cards[2].name, c.cards[2].index,
      c.cards[3].name, c.cards[3].index,
      c.cards[4].name, c.cards[4].index,
      c.cards[5].name, c.cards[5].index,
      c.cards[6].name, c.cards[6].index); */
    //combinations.push_back(c);

    // don't write to buffer
    /* {
      char currentByte = 0;
      int bitsRemaining = 8;
      for(int i=0; i<_K; i++) { // for every card
        // store card index
        if(bitsRemaining >= 6) {
          currentByte |= hand[i].index << (bitsRemaining-6);
          bitsRemaining -= 6;
          if(bitsRemaining == 0) {
            // write byte to buffer
            buffer[bufferIndex++] = currentByte;
            // reset byte
            currentByte = 0;
            bitsRemaining = 8;
          }
        }
        else {
          currentByte |= hand[i].index >> (6-bitsRemaining);
          // write byte to buffer
          buffer[bufferIndex++] = currentByte;
          // fill next byte with the remaining bits
          currentByte = hand[i].index << (8-(6-bitsRemaining));
          bitsRemaining = 8-(6-bitsRemaining);
        }
        
        // store combination value
        if(bitsRemaining >= 7) {
          currentByte |= hand[i].effects.combinationCount << (bitsRemaining-7);
          bitsRemaining -= 7;
          if(bitsRemaining == 0) {
            // write byte to buffer
            buffer[bufferIndex++] = currentByte;
            // reset byte
            currentByte = 0;
            bitsRemaining = 8;
          }
        }
        else {
          currentByte |= hand[i].effects.combinationCount >> (7-bitsRemaining);
          // write byte to buffer
          buffer[bufferIndex++] = currentByte;
          // fill next byte with the remaining bits
          currentByte = hand[i].effects.combinationCount << (8-(7-bitsRemaining));
          bitsRemaining = 8-(7-bitsRemaining);
        }
      }
      // 13 bits remaining
      // store sign bit
      currentByte |= static_cast<char>(c.value < 0) << 4;
      // store absolute value in the remaining 12 bits
      int absoluteValue = abs(c.value);
      currentByte |= (absoluteValue >> 8) & 0x0F;
      // write byte to buffer
      buffer[bufferIndex++] = currentByte;
      // write last byte to buffer
      buffer[bufferIndex++] = static_cast<char>(absoluteValue & 0xFF);
      if(bufferIndex == BUFFER_SIZE) { // only need to check at this point because bytes are written in chunks of 13, and BUFFER_SIZE is a multiple of 13
        writeToFile();
        bufferIndex = 0;
      }
    } */

    // update cards
    uint8_t replaceIndex = 0;

    /* if(hand[0].index == 0 &&
       hand[1].index == 1 &&
       hand[2].index == 2 &&
       hand[3].index == 3 &&
       hand[4].index == 4 &&
       hand[5].index == 5 &&
       hand[6].index == 8) // Höhle
      println("Debug statement"); */
    // update best cards
    /* // old version
    for(int i=0; i<10; i++)
      if(totalValue == best10[8*i+7]) {
        bool repeatEntry = true;
        for(int j=0; j<7; j++)
          if(hand[j].index != best10[8*i+j] >> 8) {
            repeatEntry = false;
            break;
          }
        if(!repeatEntry)
          replaceIndex = i;
        break;
      }
      else if(totalValue > best10[8*i+7]) {
        replaceIndex = i;
        break;
      }
    // old version end */

    for(int8_t i=9; i>=0; i--)
      if(totalValue == best10[8*i+7]) {
        bool repeatEntry = true;
        for(uint8_t j=0; j<7; j++)
          if(hand[j].index != best10[8*i+j] >> 8) {
            repeatEntry = false;
            break;
          }
        replaceIndex = repeatEntry ? 10 : i+1;
        break;
      }
      else if(totalValue < best10[8*i+7]) {
        replaceIndex = i+1;
        break;
      }

    if(replaceIndex != 10) {
      for(uint8_t i=79; i >= 8*replaceIndex+8; i--)
        best10[i] = best10[i-8];
      for(uint8_t i=0; i<7; i++)
        best10[8*replaceIndex + i] = hand[i].index << 8 | hand[i].effects.combinationCount;
      best10[8*replaceIndex + 7] = totalValue;
    }
    replaceIndex = 0;

    // update worst cards
    for(int8_t i=9; i>=0; i--)
      if(totalValue == worst10[8*i+7]) {
        bool repeatEntry = true;
        for(uint8_t j=0; j<7; j++)
          if(hand[j].index != worst10[8*i+j] >> 8) {
            repeatEntry = false;
            break;
          }
        replaceIndex = repeatEntry ? 10 : i+1;
        break;
      }
      else if(totalValue > worst10[8*i+7]) {
        replaceIndex = i+1;
        break;
      }
    
    if(replaceIndex != 10) {
      for(uint8_t i=79; i >= 8*replaceIndex+8; i--)
        worst10[i] = worst10[i-8];
      for(uint8_t i=0; i<7; i++)
        worst10[8*replaceIndex + i] = hand[i].index << 8 | hand[i].effects.combinationCount;
      worst10[8*replaceIndex + 7] = totalValue;
    }
    replaceIndex = 0;

    // update best cards per type
    for(uint8_t j=0; j<7; j++) {
      uint16_t offset = 80*hand[j].index;
      for(int8_t i=9; i>=0; i--)
        if(totalValue == best10ForEach[offset+8*i+7]) {
          bool repeatEntry = true;
          for(uint8_t j=0; j<7; j++)
            if(hand[j].index != best10ForEach[offset+8*i+j] >> 8) {
              repeatEntry = false;
              break;
            }
          replaceIndex = repeatEntry ? 10 : i+1;
          break;
        }
        else if(totalValue < best10ForEach[offset+8*i+7]) {
          replaceIndex = i+1;
          break;
        }
      
      if(replaceIndex != 10) {
        for(uint8_t i=79; i >= 8*replaceIndex+8; i--)
          best10ForEach[offset + i] = best10ForEach[offset + i-8]; // found an error here
        for(uint8_t i=0; i<7; i++)
          best10ForEach[offset + 8*replaceIndex + i] = hand[i].index << 8 | hand[i].effects.combinationCount;
        best10ForEach[offset + 8*replaceIndex + 7] = totalValue;
      }
      replaceIndex = 0;
    }

    // increase combination counter and print progress
    combinationCounter++;
    if(combinationCounter % 10'000'000 == 0) {
      // periodically back up the old file and create a new file with intermediate results

      // back up old file if existent
      if(filesystem::exists(basePath/"fantasy_realms.data"))
        filesystem::rename(basePath/"fantasy_realms.data", basePath/"fantasy_realms_backup.data");

      // create data to store in new file
      /* store:
       * 7 bytes for the current combination indices
       * best combinations: 2 * 80 bytes
       * worst combinations: 2 * 80 bytes
       * best combinations for each card: 2 * 80*52 = 2 * 4160 bytes
       * total: 8647 bytes
       */
      char buffer[8647];
      for(uint8_t i=0; i<_K; i++)
        buffer[i] = static_cast<char>(combination[i]);
      uint16_t offset = _K;
      for(uint8_t i=0; i<80; i++) {
        buffer[offset+2*i] = best10[i] >> 8;
        buffer[offset+2*i+1] = best10[i] & 0xff;
      }
      offset += 160;
      for(uint8_t i=0; i<80; i++) {
        buffer[offset+2*i] = worst10[i] >> 8;
        buffer[offset+2*i+1] = worst10[i] & 0xff;
      }
      offset += 160;
      for(uint16_t i=0; i<52*80; i++) {
        buffer[offset+2*i] = best10ForEach[i] >> 8;
        buffer[offset+2*i+1] = best10ForEach[i] & 0xff;
      }

      // create new file
      fstream f;
      f.open(basePath/"fantasy_realms.data", ios_base::out | ios::binary); // no need to truncate because the old file was moved
      f.write(buffer, 8647);

      println("{}", combinationCounter / 10'000'000); // print after writing to file!
    }
    /* if(combinationCounter > 8'800'000)
      println("{}: {} points: {} ({}), {} ({}), {} ({}), {} ({}), {} ({}), {} ({}), {} ({})", combinationCounter, totalValue,
        hand[0].name, hand[0].index,
        hand[1].name, hand[1].index,
        hand[2].name, hand[2].index,
        hand[3].name, hand[3].index,
        hand[4].name, hand[4].index,
        hand[5].name, hand[5].index,
        hand[6].name, hand[6].index); */
  }
}


int main(int argc, char** argv) {
  println("Getting file path...");
  filesystem::path filePath(argv[0]);
  basePath = filePath.remove_filename();

  println("Initializing cards...");
  initializeCards();

  println("Initializing calculation...");
  for(uint8_t i=7; i<80; i+=8) {
    worst10[i] = 100; // initialize to a value somewhat above 0 to make sure positive values are recorded, 
    // otherwise they would be discarded because they weren't lower than the starting value
  }

  /* println("Clearing file...");
  {
    fstream f;
    f.open("fantasy_realms_combinations.data", ios_base::out | ios_base::trunc);
    f.close();
  } */

  uint8_t startCombinationIndices[7] = {0, 1, 2, 3, 4, 5, 6};

  // check file integrity
  println("Checking if previous calculations exist...");
  bool previousCalculationsExist = filesystem::exists(basePath/"fantasy_realms.data");

  if(previousCalculationsExist) {
    // continue from file
    println("Loading previous data...");
    fstream f;
    f.open(basePath/"fantasy_realms.data", ios_base::in | ios_base::binary);
    char buffer[8647];
    f.read(buffer, 8647);
    if(f.gcount() != 8647) {
      println("Corrupted data. Please replace the file with a correctly formatted file or remove it. Terminating the program...");
      return EXIT_FAILURE;
    }
    
    for(uint8_t i=0; i<_K; i++)
      startCombinationIndices[i] = buffer[i];
    uint16_t offset = _K;
    for(uint8_t i=0; i<80; i++) {
      if(i%8 != 7)
        best10[i] = (buffer[offset+2*i] << 8) + buffer[offset+2*i+1];
      else {
        best10[i] = buffer[offset+2*i] << 8 | static_cast<unsigned char>(buffer[offset+2*i+1]);
        bool negative = (buffer[offset+2*i] & 0x80) != 0;
        if(negative)
          best10[i] |= 0xffff0000;
      }
    }
    offset += 160;
    for(uint8_t i=0; i<80; i++) {
      if(i%8 != 7)
        worst10[i] = (buffer[offset+2*i] << 8) + buffer[offset+2*i+1];
      else {
        worst10[i] = buffer[offset+2*i] << 8 | static_cast<unsigned char>(buffer[offset+2*i+1]);
        bool negative = (buffer[offset+2*i] & 0x80) != 0;
        if(negative)
          worst10[i] |= 0xffff0000;
      }
    }
    offset += 160;
    for(int16_t i=0; i<52*80; i++) {
      if(i%8 != 7)
        best10ForEach[i] = (buffer[offset+2*i] << 8) + buffer[offset+2*i+1];
      else {
        best10ForEach[i] = buffer[offset+2*i] << 8 | static_cast<unsigned char>(buffer[offset+2*i+1]);
        bool negative = (buffer[offset+2*i] & 0x80) != 0;
        if(negative)
          best10ForEach[i] |= 0xffff0000;
      }
    }
  }

  println("Calculating...");
  forCombinationsDo(calculate, startCombinationIndices); // //stores the results in global variable 'combinations'

  /* // write the remaining buffer to file
  {
    fstream f;
    f.open("fantasy_realms_combinations.data", ios_base::app | ios_base::out | ios_base::binary);
    f.write(buffer, bufferIndex);
    f.close();
  } */

  {
    // back up the old file and create a new file with the results

    // back up old file if existent
    if(filesystem::exists(basePath/"fantasy_realms.data"))
      filesystem::rename(basePath/"fantasy_realms.data", basePath/"fantasy_realms_backup.data");

    // create data to store in new file
    /* store:
      * 7 bytes for the current combination indices
      * best combinations: 2 * 80 bytes
      * worst combinations: 2 * 80 bytes
      * best combinations for each card: 2 * 80*52 = 2 * 4160 bytes
      * total: 8647 bytes
      */
    char buffer[8647];
    for(uint8_t i=0; i<_K; i++)
      buffer[i] = static_cast<char>(_N-_K+i);
    uint16_t offset = _K;
    for(uint8_t i=0; i<80; i++) {
      buffer[offset+2*i] = best10[i] >> 8;
      buffer[offset+2*i+1] = best10[i] & 0xff;
    }
    offset += 160;
    for(uint8_t i=0; i<80; i++) {
      buffer[offset+2*i] = worst10[i] >> 8;
      buffer[offset+2*i+1] = worst10[i] & 0xff;
    }
    offset += 160;
    for(uint16_t i=0; i<52*80; i++) {
      buffer[offset+2*i] = best10ForEach[i] >> 8;
      buffer[offset+2*i+1] = best10ForEach[i] & 0xff;
    }

    // create new file
    fstream f;
    f.open(basePath/"fantasy_realms.data", ios_base::out | ios::binary); // no need to truncate because the old file was moved
    f.write(buffer, 8647);
  }

  println("Done!");

  /*
  println("Sorting the results...");
  sort(combinations.begin(), combinations.end(), [](Combination& a, Combination& b) -> bool { return a.value > b. value; }); // sort by value, descending
  */

  // store the list to memory
  /* using the following scheme with big endian:
   * cards: 52 values - 6 Bits
   * combination: 70 values - 7 Bits
   * sum: 7*(6+7) = 91 Bits
   * 13 Bits remaining
   * value: probably < 2**12 (can be negative): - the remaining 13 Bits
   * total: 104 Bits = 13 Bytes
   */
  //println("Writing the results to a file...");
  /*
  fstream f;
  {
    f.open("fantasy_realms_combinations.data", ios_base::out | ios_base::binary);

    for(Combination& c : combinations) {
      char currentByte = 0;
      int bitsRemaining = 8;
      for(int i=0; i<_K; i++) { // for every card
        // store card index
        if(bitsRemaining >= 6) {
          currentByte |= c.cards[i].index << (bitsRemaining-6);
          bitsRemaining -= 6;
          if(bitsRemaining == 0) {
            // write byte to file
            f.put(currentByte);
            // reset byte
            currentByte = 0;
            bitsRemaining = 8;
          }
        }
        else {
          currentByte |= c.cards[i].index >> (6-bitsRemaining);
          // write byte to file
          f.put(currentByte);
          // fill next byte with the remaining bits
          currentByte = c.cards[i].index << (8-(6-bitsRemaining));
          bitsRemaining = 8-(6-bitsRemaining);
        }
        
        // store combination value
        if(bitsRemaining >= 7) {
          currentByte |= c.cards[i].effects.combinationCount << (bitsRemaining-7);
          bitsRemaining -= 7;
          if(bitsRemaining == 0) {
            // write byte to file
            f.put(currentByte);
            // reset byte
            currentByte = 0;
            bitsRemaining = 8;
          }
        }
        else {
          currentByte |= c.cards[i].effects.combinationCount >> (7-bitsRemaining);
          // write byte to file
          f.put(currentByte);
          // fill next byte with the remaining bits
          currentByte = c.cards[i].effects.combinationCount << (8-(7-bitsRemaining));
          bitsRemaining = 8-(7-bitsRemaining);
        }
      }
      // 13 bits remaining
      // store sign bit
      currentByte |= static_cast<char>(totalValue < 0) << 4;
      // store absolute value in the remaining 12 bits
      int absoluteValue = abs(totalValue);
      currentByte |= (absoluteValue >> 8) & 0x0F;
      f.put(currentByte);
      f.put(static_cast<char>(absoluteValue & 0xFF));
    }

    f.close();
  }
  */
  /*
  println("Printing the results...");
  // get the 10 best combinations + value
  println("\nBest combinations:");
  for(int i=0; i<10; i++) {
    println("{}: {} points: {} ({}), {} ({}), {} ({}), {} ({}), {} ({}), {} ({}), {} ({})", i+1, combinations[i].value,
      combinations[i].cards[0].name, combinations[i].cards[0].index,
      combinations[i].cards[1].name, combinations[i].cards[1].index,
      combinations[i].cards[2].name, combinations[i].cards[2].index,
      combinations[i].cards[3].name, combinations[i].cards[3].index,
      combinations[i].cards[4].name, combinations[i].cards[4].index,
      combinations[i].cards[5].name, combinations[i].cards[5].index,
      combinations[i].cards[6].name, combinations[i].cards[6].index);
  }

  // get the 10 worst combinations + value
  println("\nWorst combinations:");
  for(int i=0; i<10; i++) {
    int index = combinations.size()-1 - i;
    println("{}: {} points: {} ({}), {} ({}), {} ({}), {} ({}), {} ({}), {} ({}), {} ({})", i+1, combinations[index].value,
      combinations[index].cards[0].name, combinations[index].cards[0].index,
      combinations[index].cards[1].name, combinations[index].cards[1].index,
      combinations[index].cards[2].name, combinations[index].cards[2].index,
      combinations[index].cards[3].name, combinations[index].cards[3].index,
      combinations[index].cards[4].name, combinations[index].cards[4].index,
      combinations[index].cards[5].name, combinations[index].cards[5].index,
      combinations[index].cards[6].name, combinations[index].cards[6].index);
  }

  // get the 10 best combinations for every card + value
  for(int i=0; i<_N; i++) {
    println("\nBest combinations with {}:", cards[i].name);
    for(int j=0, combinationsPrinted=0; j < combinations.size() && combinationsPrinted < 10; j++) {
      bool combinationHasCard = false;
      for(int k=0; k<_K; k++)
        if(combinations[j].cards[k].index == i) {
          println("{}: {} points: {} ({}), {} ({}), {} ({}), {} ({}), {} ({}), {} ({}), {} ({})", i+1, combinations[j].value,
            combinations[j].cards[0].name, combinations[j].cards[0].index,
            combinations[j].cards[1].name, combinations[j].cards[1].index,
            combinations[j].cards[2].name, combinations[j].cards[2].index,
            combinations[j].cards[3].name, combinations[j].cards[3].index,
            combinations[j].cards[4].name, combinations[j].cards[4].index,
            combinations[j].cards[5].name, combinations[j].cards[5].index,
            combinations[j].cards[6].name, combinations[j].cards[6].index);
        }
    }
  }
  */

  return EXIT_SUCCESS;
}