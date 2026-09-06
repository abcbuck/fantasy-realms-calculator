//todo at the end: change ints to more appropriate bounds

#include <algorithm>
#include <cmath>
#include <fstream>
#include <print>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>
using namespace std;

const int _K = 7;
const int _N = 52; // ohne den Totenbeschwörer
// Den Totenbeschwörer sollte ich noch einmal mit einbeziehen, wenn alles funktioniert, weil er für das Juwel der Ordnung und den Sammler wichtig sein kann, und in diesen Fällen einen großen Unterschied macht.
// Idee: Den Totenbeschwörer zu jeder Hand hinzufügen, die eine Karte enthält, die am Ende durch ihn aufgenommen worden sein könnte.

typedef string name_t, colour_t;

const int COLOUR_COUNT = 10;
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
  int combinationCount = 1;
  bool invalid = false; // skip a combination that contains this card; used to simplify calculations where one card has alternative effects to choose from
  bool blocked = false;
  void (*specialEffect)(Card* hand, int index); // this always refers to a joker, a special non-value bonus effect or unpunishing
  unordered_set<colour_t> blockColours; // block all given colours,
  unordered_set<colour_t> blockColourExceptions; // except for some colours
  unordered_set<name_t> blockNameExceptions; // or names
  colour_t selfBlockWith;
  colour_t selfBlockWithout;
  int punishValue = 0;
  unordered_set<colour_t> punishColours; // give a point punishment for every card of the given color
  name_t punishException;
  colour_t punishForNo; // give a punishment in points if the given color isn't present
  int (*bonusPoints)(Card* hand, int index);

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
  
  bool hasPunishment() {
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
  }
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
  for(int i=0; i<_N; i++)
    cards[i].index = i;

  cards[0].name = "Gestaltwandler";
  cards[0].colour = "Joker";
  cards[0].baseValue = 0;
  cards[0].effects.hasMultipleCombinations = true;
  cards[0].effects.combinationCount = (_N-3)/2;
  cards[0].effects.specialEffect = [](Card* hand, int index) {
    unordered_set<colour_t> validColours = {"Artefakt", "Anführer", "Zauberer", "Waffe", "Bestie"};
    int count = 0;
    int target = 0;
    while(count != hand[index].effects.combinationCount || !validColours.contains(cards[target].colour)) {
      if(validColours.contains(cards[target].colour)) // the colour we're looking for
        count++;
      target++;
    }
    // select card
    hand[index].name = cards[target].name;
    hand[index].colour = cards[target].colour;
  };

  cards[1].name = "Spiegelung";
  cards[1].colour = "Joker";
  cards[1].baseValue = 0;
  cards[1].effects.hasMultipleCombinations = true;
  cards[1].effects.combinationCount = (_N-3)/2+1;
  cards[1].effects.specialEffect = [](Card* hand, int index) {
    unordered_set<colour_t> validColours = {"Armee", "Land", "Wetter", "Flut", "Flamme"};
    int count = 0;
    int target = 0;
    while(count != hand[index].effects.combinationCount || !validColours.contains(cards[target].colour)) {
      if(validColours.contains(cards[target].colour)) // the colour we're looking for
        count++;
      target++;
    }
    // select card
    hand[index].name = cards[target].name;
    hand[index].colour = cards[target].colour;
  };

  cards[2].name = "Doppelgänger";
  cards[2].colour = "Joker";
  cards[2].baseValue = 0;
  cards[2].effects.hasMultipleCombinations = true;
  cards[2].effects.combinationCount = _K-1;
  cards[2].effects.specialEffect = [](Card* hand, int index) {
    int targetIndex = hand[index].effects.combinationCount;
    if(targetIndex >= index)
      targetIndex++;
    // copy name, base value, colour
    hand[index].name      = hand[targetIndex].name;
    hand[index].baseValue = hand[targetIndex].baseValue;
    hand[index].colour    = hand[targetIndex].colour;
    // and punishment
    hand[index].effects.blockColours          = hand[targetIndex].effects.blockColours;
    hand[index].effects.blockColourExceptions = hand[targetIndex].effects.blockColourExceptions;
    hand[index].effects.blockNameExceptions   = hand[targetIndex].effects.blockNameExceptions;
    hand[index].effects.selfBlockWith         = hand[targetIndex].effects.selfBlockWith;
    hand[index].effects.selfBlockWithout      = hand[targetIndex].effects.selfBlockWithout;
    hand[index].effects.punishValue           = hand[targetIndex].effects.punishValue;
    hand[index].effects.punishColours         = hand[targetIndex].effects.punishColours;
    hand[index].effects.punishException       = hand[targetIndex].effects.punishException;
    hand[index].effects.punishForNo           = hand[targetIndex].effects.punishForNo;
  };

  // I think function pointers should be zero-initialized because they have static storage duration as they are defined globally here
  cards[3].name = "Buch der Veränderung";
  cards[3].colour = "Artefakt";
  cards[3].baseValue = 3;
  cards[3].effects.hasMultipleCombinations = true;
  cards[3].effects.combinationCount = _K*COLOUR_COUNT; //6 other cards, 10 colours, 6*10 = 60; this value changes to each of the counts in every possible combination with the other cards if the card is included
  cards[3].effects.specialEffect = [](Card* hand, int index) {
    if(hand[index].effects.combinationCount / COLOUR_COUNT == index || hand[hand[index].effects.combinationCount / COLOUR_COUNT].colour == colours[hand[index].effects.combinationCount % COLOUR_COUNT]) {
      hand[index].effects.invalid = true;
      return;
    }

    hand[hand[index].effects.combinationCount / COLOUR_COUNT].colour = colours[hand[index].effects.combinationCount % COLOUR_COUNT];
  };

  // cards 0-3 change colours
  // cards 4-10 unpunish
  // cards 10-17 block
  // cards 18-24 punish
  // cards 25-51 remaining cards

  cards[4].name = "Rune des Schutzes";
  cards[4].colour = "Artefakt";
  cards[4].baseValue = 1;
  cards[4].effects.specialEffect = [](Card* hand, int index) {
    for(int i=0; i<_K; i++)
      hand[i].effects.removePunishments();
  };

  cards[5].name = "Insel";
  cards[5].colour = "Flut";
  cards[5].baseValue = 14;
  cards[5].effects.hasMultipleCombinations = true;
  cards[5].effects.combinationCount = 4; // there are at most three cards that fulfill this card's condition,
  // also consider holding this card without using its effect as a fourth option
  cards[5].effects.specialEffect = [](Card* hand, int index) {
    int candidateIndex = hand[index].effects.combinationCount;
    if(candidateIndex == 3)
      return;
    unordered_set<colour_t> validColours = {"Flut", "Flamme"};
    int candidateCount = 0;
    for(int i=0; i<_K; i++) {
      if(validColours.contains(hand[i].colour) && hand[i].effects.hasPunishment()) {
        if(candidateCount == candidateIndex) {
          hand[i].effects.removePunishments();
          return;
        }
        candidateCount++;
      }
    }
    hand[index].effects.invalid = true;
  };

  cards[6].name = "Herr der Bestien";
  cards[6].colour = "Zauberer";
  cards[6].baseValue = 9;
  cards[6].effects.specialEffect = [](Card* hand, int index) {
    for(int i=0; i<_K; i++)
      if(hand[i].colour == "Bestie")
        hand[i].effects.removePunishments();
  };
  cards[6].effects.bonusPoints = [](Card* hand, int index) -> int {
    int sum = 0;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && hand[i].colour == "Bestie")
        sum += 9;
    return sum;
  };

  cards[7].name = "Gebirge";
  cards[7].colour = "Land";
  cards[7].baseValue = 9;
  cards[7].effects.specialEffect = [](Card* hand, int index) {
    for(int i=0; i<_K; i++)
      if(hand[i].colour == "Flut")
        hand[i].effects.removePunishments();
  };
  cards[7].effects.bonusPoints = [](Card* hand, int index) -> int {
    unordered_set<name_t> condition = {"Rauch", "Buschfeuer"};
    unordered_set<name_t> inHand;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && condition.contains(hand[i].name))
        inHand.insert(hand[i].name);
    return inHand == condition ? 50 : 0;
  };

  cards[8].name = "Höhle";
  cards[8].colour = "Land";
  cards[8].baseValue = 6;
  cards[8].effects.specialEffect = [](Card* hand, int index) {
    for(int i=0; i<_K; i++)
      if(hand[i].colour == "Wetter")
        hand[i].effects.removePunishments();
  };
  cards[8].effects.bonusPoints = [](Card* hand, int index) -> int {
    unordered_set<name_t> condition = {"Zwergeninfanterie", "Drache"};
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && condition.contains(hand[i].name))
        return 25;
    return 0;
  };

  cards[9].name = "Waldläufer";
  cards[9].colour = "Armee";
  cards[9].baseValue = 5;
  cards[9].effects.specialEffect = [](Card* hand, int index) {
    for(int i=0; i<_K; i++)
      hand[i].effects.removeFromPunishments("Armee");
  };
  cards[9].effects.bonusPoints = [](Card* hand, int index) -> int {
    int sum = 0;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && hand[i].colour == "Land")
        sum += 10;
    return sum;
  };

  cards[10].name = "Kriegsschiff";
  cards[10].colour = "Waffe";
  cards[10].baseValue = 23;
  cards[10].effects.specialEffect = [](Card* hand, int index) {
    for(int i=0; i<_K; i++)
      if(hand[i].colour == "Flut")
        hand[i].effects.removeFromPunishments("Armee");
  };
  cards[10].effects.selfBlockWithout = "Flut";

  cards[11].name = "Große Flut";
  cards[11].colour = "Flut";
  cards[11].baseValue = 32;
  cards[11].effects.blockColours = {"Armee", "Land", "Flamme"};
  cards[11].effects.blockNameExceptions = {"Gebirge", "Blitz"};

  cards[12].name = "Basilisk";
  cards[12].colour = "Bestie";
  cards[12].baseValue = 35;
  cards[12].effects.blockColours = {"Armee", "Anführer", "Bestie"};
  cards[12].effects.blockNameExceptions = {"Basilisk"};

  cards[13].name = "Buschfeuer";
  cards[13].colour = "Flamme";
  cards[13].baseValue = 40;
  cards[13].effects.blockColours.insert(colours, colours + COLOUR_COUNT);
  cards[13].effects.blockColourExceptions = {"Flamme", "Zauberer", "Wetter", "Waffe", "Artefakt"};
  cards[13].effects.blockNameExceptions = {"Gebirge", "Große Flut", "Insel", "Einhorn", "Drache"};

  cards[14].name = "Blizzard";
  cards[14].colour = "Wetter";
  cards[14].baseValue = 30;
  cards[14].effects.blockColours = {"Flut"};
  cards[14].effects.punishValue = 5;
  cards[14].effects.punishColours = {"Armee", "Anführer", "Bestie", "Flamme"};

  cards[15].name = "Rauch";
  cards[15].colour = "Wetter";
  cards[15].baseValue = 27;
  cards[15].effects.selfBlockWithout = "Flamme";

  cards[16].name = "Kampfzeppelin";
  cards[16].colour = "Waffe";
  cards[16].baseValue = 35;
  cards[16].effects.selfBlockWithout = "Armee";
  cards[16].effects.selfBlockWith = "Wetter";

  cards[17].name = "Regensturm";
  cards[17].colour = "Wetter";
  cards[17].baseValue = 8;
  cards[17].effects.blockColours = {"Flamme"};
  cards[17].effects.blockNameExceptions = {"Blitz"};
  cards[17].effects.bonusPoints = [](Card* hand, int index) -> int {
    int sum = 0;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && hand[i].colour == "Flut")
        sum += 10;
    return sum;
  };

  cards[18].name = "Hexenmeister";
  cards[18].colour = "Zauberer";
  cards[18].baseValue = 25;
  cards[18].effects.punishValue = 10;
  cards[18].effects.punishColours = {"Anführer", "Zauberer"};
  cards[18].effects.punishException = "Hexenmeister";

  cards[19].name = "Sumpf";
  cards[19].colour = "Flut";
  cards[19].baseValue = 18;
  cards[19].effects.punishValue = 3;
  cards[19].effects.punishColours = {"Armee", "Flamme"};

  cards[20].name = "Drache";
  cards[20].colour = "Bestie";
  cards[20].baseValue = 30;
  cards[20].effects.punishValue = 40;
  cards[20].effects.punishColours = {"Zauberer"};

  cards[21].name = "Kaiserin";
  cards[21].colour = "Anführer";
  cards[21].baseValue = 15;
  cards[21].effects.punishValue = 5;
  cards[21].effects.punishColours = {"Anführer"};
  cards[21].effects.punishException = "Kaiserin";

  cards[22].name = "Zwergeninfanterie";
  cards[22].colour = "Armee";
  cards[22].baseValue = 15;
  cards[22].effects.punishValue = 2;
  cards[22].effects.punishColours = {"Armee"};
  cards[22].effects.punishException = "Zwergeninfanterie";

  cards[23].name = "Leichte Kavallerie";
  cards[23].colour = "Armee";
  cards[23].baseValue = 17;
  cards[23].effects.punishValue = 2;
  cards[23].effects.punishColours = {"Land"};

  cards[24].name = "Ritter";
  cards[24].colour = "Armee";
  cards[24].baseValue = 20;
  cards[24].effects.punishValue = 8;
  cards[24].effects.punishForNo = "Anführer";

  cards[25].name = "Magierin";
  cards[25].colour = "Zauberer";
  cards[25].baseValue = 5;
  cards[25].effects.bonusPoints = [](Card* hand, int index) -> int {
    unordered_set<colour_t> validColours = {"Land", "Wetter", "Flut", "Flamme"};
    int sum = 0;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && validColours.contains(hand[i].colour))
        sum += 5;
    return sum;
  };

  cards[26].name = "Sammler";
  cards[26].colour = "Zauberer";
  cards[26].baseValue = 7;
  cards[26].effects.bonusPoints = [](Card* hand, int index) -> int {
    int maxCount = 0;

    //get max count of same colour cards in hand
    int prev = -1;
    int differentIndex = 0;
    while(differentIndex != prev) {
      prev = differentIndex;
      int count = 0;
      while(hand[differentIndex].effects.blocked) {
        prev++;
        differentIndex++;
      }
      for(int i=differentIndex; i<_K; i++) {
        if(!hand[i].effects.blocked) {
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
    if(maxCount == 5)
      return 100;
    return 0; // just in case
  };

  cards[27].name = "Quelle des Lebens";
  cards[27].colour = "Flut";
  cards[27].baseValue = 1;
  cards[27].effects.hasMultipleCombinations = true;
  cards[27].effects.combinationCount = _K-1;
  cards[27].effects.specialEffect = [](Card* hand, int index) {
      int targetIndex = hand[index].effects.combinationCount;
      if(targetIndex >= index)
        targetIndex++;
      unordered_set<colour_t> validColours = {"Waffe", "Flut", "Flamme", "Land", "Wetter"};
      if(!validColours.contains(hand[targetIndex].colour))
        hand[index].effects.invalid = true;
  };
  cards[27].effects.bonusPoints = [](Card* hand, int index) -> int {
    int targetIndex = hand[index].effects.combinationCount;
    if(targetIndex >= index)
      targetIndex++;
    // no need to check, already checked in specialEffect()
    //unordered_set<colour_t> validColours = {"Waffe", "Flut", "Flamme", "Land", "Wetter"};
    //if(validColours.contains(hand[targetIndex].colour))
    if(!hand[targetIndex].effects.blocked)
      return hand[targetIndex].baseValue;
    return 0;
  };

  cards[28].name = "Wasserwesen";
  cards[28].colour = "Flut";
  cards[28].baseValue = 4;
  cards[28].effects.bonusPoints = [](Card* hand, int index) -> int {
    int sum = 0;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && i != index && hand[i].colour == "Flut")
        sum += 15;
    return sum;
  };

  cards[29].name = "Schlachtross";
  cards[29].colour = "Bestie";
  cards[29].baseValue = 6;
  cards[29].effects.bonusPoints = [](Card* hand, int index) -> int {
    unordered_set<colour_t> validColours = {"Anführer", "Zauberer"};
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && validColours.contains(hand[i].colour))
        return 14;
    return 0;
  };

  cards[30].name = "Einhorn";
  cards[30].colour = "Bestie";
  cards[30].baseValue = 9;
  cards[30].effects.bonusPoints = [](Card* hand, int index) -> int {
    unordered_set<name_t> condition = {"Kaiserin", "Königin", "Magierin"};
    int value = 0;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked) {
        if(hand[i].name == "Prinzessin")
          return 30;
        else if(condition.contains(hand[i].name))
          value = 15;
      }
    return value;
  };

  cards[31].name = "Hydra";
  cards[31].colour = "Bestie";
  cards[31].baseValue = 12;
  cards[31].effects.bonusPoints = [](Card* hand, int index) -> int {
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && hand[i].name == "Sumpf")
        return 28;
    return 0;
  };

  cards[32].name = "Kerze";
  cards[32].colour = "Flamme";
  cards[32].baseValue = 2;
  cards[32].effects.bonusPoints = [](Card* hand, int index) -> int {
    bool book = false;
    bool tower = false;
    bool wizard = false;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked) {
        if(hand[i].name == "Buch der Veränderung")
          book = true;
        else if(hand[i].name == "Glockenturm")
          tower = true;
        if(hand[i].colour == "Zauberer")
          wizard = true;
      }
    return book && tower && wizard ? 100 : 0;
  };

  cards[33].name = "Feuerwesen";
  cards[33].colour = "Flamme";
  cards[33].baseValue = 4;
  cards[33].effects.bonusPoints = [](Card* hand, int index) -> int {
    int sum = 0;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && i != index && hand[i].colour == "Flamme")
        sum += 15;
    return sum;
  };

  cards[34].name = "Schmiede";
  cards[34].colour = "Flamme";
  cards[34].baseValue = 9;
  cards[34].effects.bonusPoints = [](Card* hand, int index) -> int {
    unordered_set<colour_t> validColours = {"Waffe", "Artefakt"};
    int sum = 0;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && validColours.contains(hand[i].colour))
        sum += 9;
    return sum;
  };

  cards[35].name = "Blitz";
  cards[35].colour = "Flamme";
  cards[35].baseValue = 11;
  cards[35].effects.bonusPoints = [](Card* hand, int index) -> int {
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && hand[i].name == "Regensturm")
        return 30;
    return 0;
  };

  cards[36].name = "Weltenbaum";
  cards[36].colour = "Artefakt";
  cards[36].baseValue = 2;
  cards[36].effects.bonusPoints = [](Card* hand, int index) -> int {
    unordered_set<colour_t> coloursInHand;
    int unblockedCount = 0;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked) {
        unblockedCount++;
        coloursInHand.insert(hand[i].colour);
      }
    return coloursInHand.size() == unblockedCount ? 50 : 0;
  };

  cards[37].name = "Schild von Keth";
  cards[37].colour = "Artefakt";
  cards[37].baseValue = 4;
  cards[37].effects.bonusPoints = [](Card* hand, int index) -> int {
    bool leader = false;
    bool sword = false;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked) {
        if(hand[i].colour == "Anführer")
          leader = true;
        if(hand[i].name == "Schwert von Keth")
          sword = true;
      }
    return leader ? (sword ? 40 : 15) : 0;
  };

  cards[38].name = "Juwel der Ordnung";
  cards[38].colour = "Artefakt";
  cards[38].baseValue = 5;
  cards[38].effects.bonusPoints = [](Card* hand, int index) -> int {
    int cardValues[_K];
    for(int i=0; i<_K; i++)
      if(hand[i].effects.blocked)
        cardValues[i] = -100;
      else
        cardValues[i] = hand[i].baseValue;
    sort(cardValues, cardValues+_K);

    int straight = 1, maxStraight = 1;
    for(int i=1; i<_K; i++) {
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

  cards[39].name = "Prinzessin";
  cards[39].colour = "Anführer";
  cards[39].baseValue = 2;
  cards[39].effects.bonusPoints = [](Card* hand, int index) -> int {
    unordered_set<colour_t> validColours = {"Armee", "Zauberer", "Anführer"};
    int sum = 0;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && i != index && validColours.contains(hand[i].colour))
        sum += 8;
    return sum;
  };

  cards[40].name = "Kriegsherr";
  cards[40].colour = "Anführer";
  cards[40].baseValue = 4;
  cards[40].effects.bonusPoints = [](Card* hand, int index) -> int {
    int sum = 0;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && hand[i].colour == "Armee")
        sum += hand[i].baseValue;
    return sum;
  };

  cards[41].name = "Königin";
  cards[41].colour = "Anführer";
  cards[41].baseValue = 6;
  cards[41].effects.bonusPoints = [](Card* hand, int index) -> int {
    int sum = 0;
    bool king = false;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked) {
        if(hand[i].colour == "Armee")
          sum += 5;
        if(hand[i].name == "König")
          king = true;
      }
    if(king)
      sum *= 4;
    return sum;
  };

  cards[42].name = "König";
  cards[42].colour = "Anführer";
  cards[42].baseValue = 8;
  cards[42].effects.bonusPoints = [](Card* hand, int index) -> int {
    int sum = 0;
    bool queen = false;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked) {
        if(hand[i].colour == "Armee")
          sum += 5;
        if(hand[i].name == "Königin")
          queen = true;
      }
    if(queen)
      sum *= 4;
    return sum;
  };

  cards[43].name = "Erdwesen";
  cards[43].colour = "Land";
  cards[43].baseValue = 4;
  cards[43].effects.bonusPoints = [](Card* hand, int index) -> int {
    int sum = 0;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && i != index && hand[i].colour == "Land")
        sum += 15;
    return sum;
  };

  cards[44].name = "Wald";
  cards[44].colour = "Land";
  cards[44].baseValue = 7;
  cards[44].effects.bonusPoints = [](Card* hand, int index) -> int {
    int sum = 0;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && (hand[i].colour == "Bestie" || hand[i].name == "Elbenschützen"))
        sum += 12;
    return sum;
  };

  cards[45].name = "Glockenturm";
  cards[45].colour = "Land";
  cards[45].baseValue = 8;
  cards[45].effects.bonusPoints = [](Card* hand, int index) -> int {
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && hand[i].colour == "Zauberer")
        return 15;
    return 0;
  };

  cards[46].name = "Luftwesen";
  cards[46].colour = "Wetter";
  cards[46].baseValue = 4;
  cards[46].effects.bonusPoints = [](Card* hand, int index) -> int {
    int sum = 0;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && i != index && hand[i].colour == "Wetter")
        sum += 15;
    return sum;
  };

  cards[47].name = "Wirbelsturm";
  cards[47].colour = "Wetter";
  cards[47].baseValue = 13;
  cards[47].effects.bonusPoints = [](Card* hand, int index) -> int {
    bool rainStorm = false;
    bool blizzard = false;
    bool greatFlood = false;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked) {
        if(hand[i].name == "Regensturm")
          rainStorm = true;
        else if(hand[i].name == "Blizzard")
          blizzard = true;
        else if(hand[i].name == "Große Flut")
          greatFlood = true;
      }
    return rainStorm && (blizzard || greatFlood) ? 40 : 0;
  };

  cards[48].name = "Elbenschützen";
  cards[48].colour = "Armee";
  cards[48].baseValue = 10;
  cards[48].effects.bonusPoints = [](Card* hand, int index) -> int {
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && hand[i].colour == "Wetter")
        return 0;
    return 5;
  };

  cards[49].name = "Zauberstab";
  cards[49].colour = "Waffe";
  cards[49].baseValue = 1;
  cards[49].effects.bonusPoints = [](Card* hand, int index) -> int {
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && hand[i].colour == "Zauberer")
        return 25;
    return 0;
  };

  cards[50].name = "Elbischer Bogen";
  cards[50].colour = "Waffe";
  cards[50].baseValue = 3;
  cards[50].effects.bonusPoints = [](Card* hand, int index) -> int {
    unordered_set<name_t> condition = {"Elbenschützen", "Kriegsherr", "Herr der Bestien"};
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked && condition.contains(hand[i].name))
        return 30;
    return 0;
  };

  cards[51].name = "Schwert von Keth";
  cards[51].colour = "Waffe";
  cards[51].baseValue = 7;
  cards[51].effects.bonusPoints = [](Card* hand, int index) -> int {
    bool leader = false;
    bool shield = false;
    for(int i=0; i<_K; i++)
      if(!hand[i].effects.blocked) {
        if(hand[i].colour == "Anführer")
          leader = true;
        if(hand[i].name == "Schild von Keth")
          shield = true;
      }
    return leader ? (shield ? 40 : 10) : 0;
  };
}

struct Combination {
  Card cards[_K];
  int value;
};

//vector<Combination> combinations;
const int BUFFER_SIZE = 1048580; // a multiple of 13; 1 MiB + 4 Bytes
char buffer[BUFFER_SIZE];
int bufferIndex = 0;

void writeToFile() {
  fstream f;
  f.open("fantasy_realms_combinations.data", ios_base::app | ios_base::out | ios_base::binary);
  f.write(buffer, BUFFER_SIZE);
  f.close();
}


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
void forCombinationsDo(void (*hand_fn)(int*)) {
  int combination[_K]; // = {4, 5, 6, 7, 8, 9, 10};
  for(int i=0; i<_K; i++)
    combination[i] = i;
  combination[_K-1] = 26; //delete
  hand_fn(combination);
  int i = 0;
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

bool nextSelection(int* selection, int* limits) {
  int i=0;
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
int combinationCounter = 0;
void calculate(int* combination) {
  // calculate the value of the combination
  Card hand[_K];
  int selectionLimits[_K];
  for(int i=0; i<_K; i++) {
    hand[i] = cards[combination[i]]; // copy card
    // add multiple combinations for relevant cards!
    selectionLimits[i] = hand[i].effects.combinationCount;
  }
  //iterate through the cartesian product of card options
  int selection[_K] = {};
  for(bool isValidSelection = true; isValidSelection; isValidSelection = nextSelection(selection, selectionLimits)) {
    //reset cards for every selection
    for(int i=0; i<_K; i++) {
      hand[i] = cards[combination[i]]; // copy card
    }
    for(int i=0; i<_K; i++)
      if(hand[i].effects.hasMultipleCombinations)
        hand[i].effects.combinationCount = selection[i];
    // apply effects and calculate
    int totalValue = 0;
    {
      // do each of the following for every hand card before continuing to the next step

      // special effects (includes unblocking)
      for(int i=0; i<_K; i++)
        if(hand[i].effects.specialEffect != NULL)
          hand[i].effects.specialEffect(hand, i);
      // skip combinations with invalid=true
      bool invalid = false;
      for(int i=0; i<_K; i++)
        if(hand[i].effects.invalid) {
          invalid = true;
          break;
        }
      if(invalid)
        continue;

      // block
      for(int i=0; i<_K; i++) {
        // block other cards
        if(!hand[i].effects.blockColours.empty())
          for(int j=0; j<_K; j++)
            if(hand[i].effects.blockColours.contains(hand[j].colour) && // block all given colours,
            !hand[i].effects.blockColourExceptions.contains(hand[j].colour) && // except for some colours
            !hand[i].effects.blockNameExceptions.contains(hand[j].name)) // or names
              hand[j].effects.blocked = true;

        // self-block
        if(!hand[i].effects.blocked && hand[i].effects.selfBlockWith != "")
          for(int j=0; j<_K; j++)
            if(hand[j].colour == hand[i].effects.selfBlockWith) {
              hand[i].effects.blocked = true;
              break;
            }
        if(!hand[i].effects.blocked && hand[i].effects.selfBlockWithout != "") {
          bool block = true;
          for(int j=0; j<_K; j++)
            if(hand[j].colour == hand[i].effects.selfBlockWithout) {
              block = false;
              break;
            }
          if(block)
            hand[i].effects.blocked = true;
        }
      }

      // punish (if not blocked)
      for(int i=0; i<_K; i++) {
        if(!hand[i].effects.blocked) {
          if(hand[i].effects.punishForNo != "") {
            bool punish = true;
            for(int j=0; j<_K; j++)
              if(hand[j].colour == hand[i].effects.punishForNo) {
                punish = false;
                break;
              }
            if(punish)
              totalValue -= hand[i].effects.punishValue;
          }
          else for(int j=0; j<_K; j++) {
            if(!hand[j].effects.blocked &&
            hand[i].effects.punishColours.contains(hand[j].colour) &&
            hand[j].name != hand[i].effects.punishException)
              totalValue -= hand[i].effects.punishValue;
          }
        }
      }

      // bonus (for non-blocked cards)
      for(int i=0; i<_K; i++)
        if(!hand[i].effects.blocked) {
          totalValue += hand[i].baseValue;
          if(hand[i].effects.bonusPoints != NULL)
            totalValue += hand[i].effects.bonusPoints(hand, i);
        }
    }

    // store the combination + value in an ordered list, ordered by total value descending
    Combination c;
    copy(hand, hand+_K, c.cards);
    c.value = totalValue;

    /* println("{} points: {} ({}), {} ({}), {} ({}), {} ({}), {} ({}), {} ({}), {} ({})", totalValue,
      c.cards[0].name, c.cards[0].index,
      c.cards[1].name, c.cards[1].index,
      c.cards[2].name, c.cards[2].index,
      c.cards[3].name, c.cards[3].index,
      c.cards[4].name, c.cards[4].index,
      c.cards[5].name, c.cards[5].index,
      c.cards[6].name, c.cards[6].index); */
    //combinations.push_back(c);

    {
      char currentByte = 0;
      int bitsRemaining = 8;
      for(int i=0; i<_K; i++) { // for every card
        // store card index
        if(bitsRemaining >= 6) {
          currentByte |= c.cards[i].index << (bitsRemaining-6);
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
          currentByte |= c.cards[i].index >> (6-bitsRemaining);
          // write byte to buffer
          buffer[bufferIndex++] = currentByte;
          // fill next byte with the remaining bits
          currentByte = c.cards[i].index << (8-(6-bitsRemaining));
          bitsRemaining = 8-(6-bitsRemaining);
        }
        
        // store combination value
        if(bitsRemaining >= 7) {
          currentByte |= c.cards[i].effects.combinationCount << (bitsRemaining-7);
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
          currentByte |= c.cards[i].effects.combinationCount >> (7-bitsRemaining);
          // write byte to buffer
          buffer[bufferIndex++] = currentByte;
          // fill next byte with the remaining bits
          currentByte = c.cards[i].effects.combinationCount << (8-(7-bitsRemaining));
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
    }
    // increase combination counter and print if == 0 % 100'000
    combinationCounter++;
    if(combinationCounter % 100'000 == 0)
      println("{}", combinationCounter / 100'000);
    /* if(combinationCounter > 8'800'000)
      println("{}: {} points: {} ({}), {} ({}), {} ({}), {} ({}), {} ({}), {} ({}), {} ({})", combinationCounter, c.value,
        c.cards[0].name, c.cards[0].index,
        c.cards[1].name, c.cards[1].index,
        c.cards[2].name, c.cards[2].index,
        c.cards[3].name, c.cards[3].index,
        c.cards[4].name, c.cards[4].index,
        c.cards[5].name, c.cards[5].index,
        c.cards[6].name, c.cards[6].index); */
  }
}

int main() {
  println("Initializing cards...");
  initializeCards();

  println("Clearing file...");
  {
    fstream f;
    f.open("fantasy_realms_combinations.data", ios_base::out | ios_base::trunc);
    f.close();
  }

  println("Calculating...");
  forCombinationsDo(calculate); //stores the results in global variable 'combinations'

  // write the remaining buffer to file
  {
    fstream f;
    f.open("fantasy_realms_combinations.data", ios_base::app | ios_base::out | ios_base::binary);
    f.write(buffer, bufferIndex);
    f.close();
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
      currentByte |= static_cast<char>(c.value < 0) << 4;
      // store absolute value in the remaining 12 bits
      int absoluteValue = abs(c.value);
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

  return 0;
}