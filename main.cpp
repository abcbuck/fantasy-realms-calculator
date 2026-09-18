#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <print>
#include <string>
using namespace std;

filesystem::path basePath;

int16_t best10[10*8] = {}; // 8 = 7 cards + 1 value
int16_t worst10[10*8] = {};

int16_t best10ForEach[10*8*52] = {};

const uint8_t _K = 7;
const uint8_t _N = 52; // without the necromancer
// I should add the necromancer later, when everything works, because he may be important to the gem of order and the collector and make a big difference in these cases.
// idea: add the necromancer to every hand that contains a card which could have been added by him in the end

typedef string name_t, colour_t;

const uint8_t COLOUR_COUNT = 10;
colour_t colours[COLOUR_COUNT] = { // for the book of changes
  "Army",
  "Artifact",
  "Beast",
  "Flame",
  "Flood",
  "Land",
  "Leader",
  "Weapon",
  "Weather",
  "Wizard"
};

struct Card;

struct Effects {
  bool hasMultipleCombinations = false;
  uint8_t combinationCount = 1;
  bool invalid = false; // skip a combination that contains this card; used to simplify calculations where one card has alternative effects to choose from
  bool blanked = false;
  void (*specialEffect)(Card* hand, uint8_t index); // this always refers to a Wild, the book of changes, unpunishing or an invalidating effect; nothing else

  void (*blank)(Card* hand, uint8_t index);
  int16_t (*punish)(Card* hand, uint8_t index);
  bool punishArmies = true; // indicates this card may punish army cards; if set to false, this punishment may be deleted
  
  int16_t (*bonusPoints)(Card* hand, uint8_t index);
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

  cards[0].name = "Doppelgänger"; // Collector references Doppelgänger as having index 0! Be sure to change this reference, should you ever change the doppelganger index.
  cards[0].colour = "Wild";
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
  };

  cards[1].name = "Mirage";
  cards[1].colour = "Wild";
  cards[1].baseValue = 0;
  cards[1].effects.hasMultipleCombinations = true;
  cards[1].effects.combinationCount = (_N-3)/2+2;
  cards[1].effects.specialEffect = [](Card* hand, uint8_t index) {
    if(hand[index].effects.combinationCount == (_N-3)/2+1)
      return; //don't use the card effect
    const char* validColours[] = {"Army", "Land", "Weather", "Flood", "Flame"};
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

  cards[2].name = "Shapeshifter";
  cards[2].colour = "Wild";
  cards[2].baseValue = 0;
  cards[2].effects.hasMultipleCombinations = true;
  cards[2].effects.combinationCount = (_N-3)/2+1;
  cards[2].effects.specialEffect = [](Card* hand, uint8_t index) {
    if(hand[index].effects.combinationCount == (_N-3)/2)
      return; //don't use the card effect
    const char* validColours[] = {"Artifact", "Leader", "Wizard", "Weapon", "Beast"};
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

  cards[3].name = "Book of Changes";
  cards[3].colour = "Artifact";
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

  cards[4].name = "Protection Rune";
  cards[4].colour = "Artifact";
  cards[4].baseValue = 1;
  cards[4].effects.specialEffect = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++) {
      hand[i].effects.blank = NULL;
      hand[i].effects.punish = NULL;
    }
  };

  cards[5].name = "World Tree";
  cards[5].colour = "Artifact";
  cards[5].baseValue = 2;
  cards[5].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
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

  cards[6].name = "Shield of Keth";
  cards[6].colour = "Artifact";
  cards[6].baseValue = 4;
  cards[6].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    bool leader = false;
    bool sword = false;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        if(hand[i].colour == "Leader")
          leader = true;
        if(hand[i].name == "Sword of Keth")
          sword = true;
      }
    return leader ? (sword ? 40 : 15) : 0;
  };

  cards[7].name = "Gem of Order";
  cards[7].colour = "Artifact";
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

  cards[8].name = "Enchantress";
  cards[8].colour = "Wizard";
  cards[8].baseValue = 5;
  cards[8].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    const char* validColours[] = {"Land", "Weather", "Flood", "Flame"};
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && find(validColours, validColours+4, hand[i].colour) != validColours+4)
        sum += 5;
    return sum;
  };

  cards[9].name = "Collector";
  cards[9].colour = "Wizard";
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

  cards[10].name = "Beastmaster";
  cards[10].colour = "Wizard";
  cards[10].baseValue = 9;
  cards[10].effects.specialEffect = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].colour == "Beast") {
        hand[i].effects.blank = NULL;
        hand[i].effects.punish = NULL;
      }
  };
  cards[10].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Beast")
        sum += 9;
    return sum;
  };

  cards[11].name = "Warlock Lord";
  cards[11].colour = "Wizard";
  cards[11].baseValue = 25;
  cards[11].effects.punish = [](Card* hand, uint8_t index) -> int16_t {
    int8_t punishment = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && (hand[i].colour == "Leader" || hand[i].colour == "Wizard") && i != index)
        punishment -= 10;
    return punishment;
  };

  cards[12].name = "Air Elemental";
  cards[12].colour = "Weather";
  cards[12].baseValue = 4;
  cards[12].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && i != index && hand[i].colour == "Weather")
        sum += 15;
    return sum;
  };

  cards[13].name = "Rainstorm";
  cards[13].colour = "Weather";
  cards[13].baseValue = 8;
  cards[13].effects.blank = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].colour == "Flame" && hand[i].name != "Lightning")
        hand[i].effects.blanked = true;
  };
  cards[13].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Flood")
        sum += 10;
    return sum;
  };

  cards[14].name = "Whirlwind";
  cards[14].colour = "Weather";
  cards[14].baseValue = 13;
  cards[14].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    bool rainStorm = false;
    bool blizzard = false;
    bool greatFlood = false;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        if(hand[i].name == "Rainstorm")
          rainStorm = true;
        else if(hand[i].name == "Blizzard")
          blizzard = true;
        else if(hand[i].name == "Great Flood")
          greatFlood = true;
      }
    return rainStorm && (blizzard || greatFlood) ? 40 : 0;
  };

  cards[15].name = "Smoke";
  cards[15].colour = "Weather";
  cards[15].baseValue = 27;
  cards[15].effects.blank = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].colour == "Flame")
        return;
    hand[index].effects.blanked = true;
  };

  cards[16].name = "Blizzard";
  cards[16].colour = "Weather";
  cards[16].baseValue = 30;
  cards[16].effects.blank = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].colour == "Flood")
        hand[i].effects.blanked = true;
  };
  cards[16].effects.punish = [](Card* hand, uint8_t index) -> int16_t {
    const char* condition[] = {"Army", "Leader", "Beast", "Flame"};
    int8_t punishment = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && find(condition, condition+4, hand[i].colour) != condition+4)
        punishment -= 5;
    return punishment;
  };

  cards[17].name = "Fountain of Life";
  cards[17].colour = "Flood";
  cards[17].baseValue = 1;
  cards[17].effects.hasMultipleCombinations = true;
  cards[17].effects.combinationCount = _K-1;
  cards[17].effects.specialEffect = [](Card* hand, uint8_t index) {
      uint8_t targetIndex = hand[index].effects.combinationCount;
      if(targetIndex >= index)
        targetIndex++;
      const char* validColours[] = {"Weapon", "Flood", "Flame", "Land", "Weather"};
      if(find(validColours, validColours+5, hand[targetIndex].colour) == validColours+5)
        hand[index].effects.invalid = true;
  };
  cards[17].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t targetIndex = hand[index].effects.combinationCount;
    if(targetIndex >= index)
      targetIndex++;
    if(!hand[targetIndex].effects.blanked)
      return hand[targetIndex].baseValue;
    return 0;
  };

  cards[18].name = "Water Elemental";
  cards[18].colour = "Flood";
  cards[18].baseValue = 4;
  cards[18].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && i != index && hand[i].colour == "Flood")
        sum += 15;
    return sum;
  };

  cards[19].name = "Island";
  cards[19].colour = "Flood";
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
      if((hand[i].colour == "Flood" || hand[i].colour == "Flame") && (hand[i].effects.blank != NULL || hand[i].effects.punish != NULL)) {
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

  cards[20].name = "Swamp";
  cards[20].colour = "Flood";
  cards[20].baseValue = 18;
  cards[20].effects.punish = [](Card* hand, uint8_t index) -> int16_t {
    int8_t punishment = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && (hand[index].effects.punishArmies && hand[i].colour == "Army" || hand[i].colour == "Flame"))
        punishment -= 3;
    return punishment;
  };

  cards[21].name = "Great Flood";
  cards[21].colour = "Flood";
  cards[21].baseValue = 32;
  cards[21].effects.blank = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[index].effects.punishArmies && hand[i].colour == "Army" ||
          hand[i].colour == "Land" && hand[i].name != "Mountain" ||
          hand[i].colour == "Flame" && hand[i].name != "Lightning")
        hand[i].effects.blanked = true;
  };

  cards[22].name = "Candle";
  cards[22].colour = "Flame";
  cards[22].baseValue = 2;
  cards[22].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    bool book = false;
    bool tower = false;
    bool wizard = false;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        if(hand[i].name == "Book of Changes")
          book = true;
        else if(hand[i].name == "Bell Tower")
          tower = true;
        if(hand[i].colour == "Wizard")
          wizard = true;
      }
    return book && tower && wizard ? 100 : 0;
  };

  cards[23].name = "Fire Elemental";
  cards[23].colour = "Flame";
  cards[23].baseValue = 4;
  cards[23].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && i != index && hand[i].colour == "Flame")
        sum += 15;
    return sum;
  };

  cards[24].name = "Forge";
  cards[24].colour = "Flame";
  cards[24].baseValue = 9;
  cards[24].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && (hand[i].colour == "Weapon" || hand[i].colour == "Artifact"))
        sum += 9;
    return sum;
  };

  cards[25].name = "Lightning";
  cards[25].colour = "Flame";
  cards[25].baseValue = 11;
  cards[25].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].name == "Rainstorm")
        return 30;
    return 0;
  };

  cards[26].name = "Wildfire";
  cards[26].colour = "Flame";
  cards[26].baseValue = 40;
  cards[26].effects.blank = [](Card* hand, uint8_t index) {
    if(hand[index].effects.blanked)
      return;
    const char* blockColourExceptions[] = {"Flame", "Wizard", "Weather", "Weapon", "Artifact"};
    const char* blockNameExceptions[] = {"Mountain", "Great Flood", "Island", "Unicorn", "Dragon"};
    for(uint8_t i=0; i<_K; i++)
      if(find(blockColourExceptions, blockColourExceptions+5, hand[i].colour) == blockColourExceptions+5 &&
          find(blockNameExceptions, blockNameExceptions+5, hand[i].name) == blockNameExceptions+5)
        hand[i].effects.blanked = true;
  };

  cards[27].name = "Warhorse";
  cards[27].colour = "Beast";
  cards[27].baseValue = 6;
  cards[27].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && (hand[i].colour == "Leader" || hand[i].colour == "Wizard"))
        return 14;
    return 0;
  };

  cards[28].name = "Unicorn";
  cards[28].colour = "Beast";
  cards[28].baseValue = 9;
  cards[28].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    const char* condition[] = {"Empress", "Queen", "Enchantress"};
    uint8_t value = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        if(hand[i].name == "Princess")
          return 30;
        else if(find(condition, condition+3, hand[i].name) != condition+3)
          value = 15;
      }
    return value;
  };

  cards[29].name = "Hydra";
  cards[29].colour = "Beast";
  cards[29].baseValue = 12;
  cards[29].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].name == "Swamp")
        return 28;
    return 0;
  };

  cards[30].name = "Dragon";
  cards[30].colour = "Beast";
  cards[30].baseValue = 30;
  cards[30].effects.punish = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Wizard")
        return 0;
    return -40;
  };

  cards[31].name = "Basilisk";
  cards[31].colour = "Beast";
  cards[31].baseValue = 35;
  cards[31].effects.blank = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[index].effects.punishArmies && hand[i].colour == "Army" ||
          hand[i].colour == "Leader" ||
          i != index && hand[i].colour == "Beast")
        hand[i].effects.blanked = true;
  };

  cards[32].name = "Magic Wand";
  cards[32].colour = "Weapon";
  cards[32].baseValue = 1;
  cards[32].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Wizard")
        return 25;
    return 0;
  };

  cards[33].name = "Elven Longbow";
  cards[33].colour = "Weapon";
  cards[33].baseValue = 3;
  cards[33].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    const char* condition[] = {"Elven Archers", "Warlord", "Beastmaster"};
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && find(condition, condition+3, hand[i].name) != condition+3)
        return 30;
    return 0;
  };

  cards[34].name = "Sword of Keth";
  cards[34].colour = "Weapon";
  cards[34].baseValue = 7;
  cards[34].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    bool leader = false;
    bool shield = false;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        if(hand[i].colour == "Leader")
          leader = true;
        if(hand[i].name == "Shield of Keth")
          shield = true;
      }
    return leader ? (shield ? 40 : 10) : 0;
  };

  cards[35].name = "Warship";
  cards[35].colour = "Weapon";
  cards[35].baseValue = 23;
  cards[35].effects.specialEffect = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].colour == "Flood")
        hand[i].effects.punishArmies = false;
  };
  cards[35].effects.blank = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].colour == "Flood")
        return;
    hand[index].effects.blanked = true;
  };

  cards[36].name = "War Dirigible";
  cards[36].colour = "Weapon";
  cards[36].baseValue = 35;
  cards[36].effects.blank = [](Card* hand, uint8_t index) {
    bool armyInHand = false;
    for(uint8_t i=0; i<_K; i++) {
      if(hand[i].colour == "Army")
        armyInHand = true;
      else if(hand[i].colour == "Weather") {
        hand[index].effects.blanked = true;
        return;
      }
    }
    if(hand[index].effects.punishArmies && !armyInHand)
      hand[index].effects.blanked = true;
  };

  cards[37].name = "Princess";
  cards[37].colour = "Leader";
  cards[37].baseValue = 2;
  cards[37].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    const char* validColours[] = {"Army", "Wizard", "Leader"};
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && i != index && find(validColours, validColours+3, hand[i].colour) != validColours+3)
        sum += 8;
    return sum;
  };

  cards[38].name = "Warlord";
  cards[38].colour = "Leader";
  cards[38].baseValue = 4;
  cards[38].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Army")
        sum += hand[i].baseValue;
    return sum;
  };

  cards[39].name = "Queen";
  cards[39].colour = "Leader";
  cards[39].baseValue = 6;
  cards[39].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    bool king = false;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        if(hand[i].colour == "Army")
          sum += 5;
        if(hand[i].name == "King")
          king = true;
      }
    if(king)
      sum *= 4;
    return sum;
  };

  cards[40].name = "King";
  cards[40].colour = "Leader";
  cards[40].baseValue = 8;
  cards[40].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    bool queen = false;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        if(hand[i].colour == "Army")
          sum += 5;
        if(hand[i].name == "Queen")
          queen = true;
      }
    if(queen)
      sum *= 4;
    return sum;
  };

  cards[41].name = "Empress";
  cards[41].colour = "Leader";
  cards[41].baseValue = 15;
  cards[41].effects.punish = [](Card* hand, uint8_t index) -> int16_t {
    int8_t punishment = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && i != index && hand[i].colour == "Leader")
        punishment -= 5;
    return punishment;
  };

  cards[42].name = "Earth Elemental";
  cards[42].colour = "Land";
  cards[42].baseValue = 4;
  cards[42].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && i != index && hand[i].colour == "Land")
        sum += 15;
    return sum;
  };

  cards[43].name = "Cavern";
  cards[43].colour = "Land";
  cards[43].baseValue = 6;
  cards[43].effects.specialEffect = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].colour == "Weather") {
        hand[i].effects.blank = NULL;
        hand[i].effects.punish = NULL;
      }
  };
  cards[43].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && (hand[i].name == "Dwarvish Infantry" || hand[i].name == "Dragon"))
        return 25;
    return 0;
  };

  cards[44].name = "Forest";
  cards[44].colour = "Land";
  cards[44].baseValue = 7;
  cards[44].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    uint8_t sum = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && (hand[i].colour == "Beast" || hand[i].name == "Elven Archers"))
        sum += 12;
    return sum;
  };

  cards[45].name = "Bell Tower";
  cards[45].colour = "Land";
  cards[45].baseValue = 8;
  cards[45].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Wizard")
        return 15;
    return 0;
  };

  cards[46].name = "Mountain";
  cards[46].colour = "Land";
  cards[46].baseValue = 9;
  cards[46].effects.specialEffect = [](Card* hand, uint8_t index) {
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].colour == "Flood") {
        hand[i].effects.blank = NULL;
        hand[i].effects.punish = NULL;
      }
  };
  cards[46].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    bool smoke = false;
    bool wildfire = false;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked) {
        if(hand[i].name == "Smoke")
          smoke = true;
        else if(hand[i].name == "Wildfire") {
          wildfire = true;
        }
      }
    return smoke && wildfire ? 50 : 0;
  };

  cards[47].name = "Rangers";
  cards[47].colour = "Army";
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

  cards[48].name = "Elven Archers";
  cards[48].colour = "Army";
  cards[48].baseValue = 10;
  cards[48].effects.bonusPoints = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Weather")
        return 0;
    return 5;
  };

  cards[49].name = "Dwarvish Infantry";
  cards[49].colour = "Army";
  cards[49].baseValue = 15;
  cards[49].effects.punish = [](Card* hand, uint8_t index) -> int16_t {
    int8_t punishment = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && i != index && hand[i].colour == "Army")
        punishment -= 2;
    return punishment;
  };

  cards[50].name = "Light Cavalry";
  cards[50].colour = "Army";
  cards[50].baseValue = 17;
  cards[50].effects.punish = [](Card* hand, uint8_t index) -> int16_t {
    int8_t punishment = 0;
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Land")
        punishment -= 2;
    return punishment;
  };

  cards[51].name = "Knights";
  cards[51].colour = "Army";
  cards[51].baseValue = 20;
  cards[51].effects.punish = [](Card* hand, uint8_t index) -> int16_t {
    for(uint8_t i=0; i<_K; i++)
      if(!hand[i].effects.blanked && hand[i].colour == "Leader")
        return 0;
    return -8;
  };
}

void readFromFile(int16_t* best10, int16_t* worst10, int16_t* best10ForEach, uint8_t* startCombinationIndices=NULL) {
  bool previousCalculationsExist = filesystem::exists(basePath/"fantasy_realms.data");

  if(previousCalculationsExist) {
    // continue from file
    if(startCombinationIndices != NULL)
      println("Reading from file...");
    fstream f;
    f.open(basePath/"fantasy_realms.data", ios_base::in | ios_base::binary);
    char buffer[8647];
    f.read(buffer, 8647);
    if(f.gcount() != 8647)
      println("Corrupted data. Ignoring existing data and creating a new file...");
    else { // interpret data and update; update code adapted from calculate function
      int16_t newBest10[80] = {};
      int16_t newWorst10[80] = {};
      int16_t newBest10ForEach[4160] = {};

      if(startCombinationIndices != NULL)
        for(uint8_t i=0; i<_K; i++)
          startCombinationIndices[i] = buffer[i];
      uint16_t offset = _K;
      uint8_t replaceIndex = 0;
      for(uint8_t i=0; i<80; i++) {
        if(i%8 != 7)
          newBest10[i] = (buffer[offset+2*i] << 8) + buffer[offset+2*i+1];
        else {
          newBest10[i] = buffer[offset+2*i] << 8 | static_cast<unsigned char>(buffer[offset+2*i+1]);
          bool negative = (buffer[offset+2*i] & 0x80) != 0;
          if(negative)
            newBest10[i] |= 0xffff0000;

          // update existing data in memory with data from file
          for(int8_t k=9; k>=0; k--)
            if(newBest10[i] == best10[8*k+7]) {
              bool repeatEntry = true;
              for(uint8_t j=0; j<7; j++)
                if(newBest10[i-7+j] >> 8 != best10[8*k+j] >> 8) {
                  repeatEntry = false;
                  break;
                }
              replaceIndex = repeatEntry ? 10 : k+1;
              if(repeatEntry)
                break;
            }
            else if(newBest10[i] < best10[8*k+7]) {
              replaceIndex = k+1;
              break;
            }
  
          if(replaceIndex != 10) {
            for(uint8_t k=79; k >= 8*replaceIndex+8; k--)
              best10[k] = best10[k-8];
            for(uint8_t k=0; k<7; k++)
              best10[8*replaceIndex + k] = newBest10[i-7+k];
            best10[8*replaceIndex + 7] = newBest10[i];
          }
          replaceIndex = 0;
        }
      }
      offset += 160;
      for(uint8_t i=0; i<80; i++) {
        if(i%8 != 7)
          newWorst10[i] = (buffer[offset+2*i] << 8) + buffer[offset+2*i+1];
        else {
          newWorst10[i] = buffer[offset+2*i] << 8 | static_cast<unsigned char>(buffer[offset+2*i+1]);
          bool negative = (buffer[offset+2*i] & 0x80) != 0;
          if(negative)
            newWorst10[i] |= 0xffff0000;

          // update existing data in memory with data from file
          for(int8_t k=9; k>=0; k--)
            if(newWorst10[i] == worst10[8*k+7]) {
              bool repeatEntry = true;
              for(uint8_t j=0; j<7; j++)
                if(newWorst10[i-7+j] >> 8 != worst10[8*k+j] >> 8) {
                  repeatEntry = false;
                  break;
                }
              replaceIndex = repeatEntry ? 10 : k+1;
              if(repeatEntry)
                break;
            }
            else if(newWorst10[i] > worst10[8*k+7]) {
              replaceIndex = k+1;
              break;
            }
  
          if(replaceIndex != 10) {
            for(uint8_t k=79; k >= 8*replaceIndex+8; k--)
              worst10[k] = worst10[k-8];
            for(uint8_t k=0; k<7; k++)
              worst10[8*replaceIndex + k] = newWorst10[i-7+k];
            worst10[8*replaceIndex + 7] = newWorst10[i];
          }
          replaceIndex = 0;
        }
      }
      offset += 160;
      for(int16_t i=0; i<52*80; i++) {
        if(i%8 != 7)
          newBest10ForEach[i] = (buffer[offset+2*i] << 8) + buffer[offset+2*i+1];
        else {
          newBest10ForEach[i] = buffer[offset+2*i] << 8 | static_cast<unsigned char>(buffer[offset+2*i+1]);
          bool negative = (buffer[offset+2*i] & 0x80) != 0;
          if(negative)
            newBest10ForEach[i] |= 0xffff0000;

          // update existing data in memory with data from file
          uint16_t offset = i/80*80;
          for(int8_t k=9; k>=0; k--)
            if(newBest10ForEach[i] == best10ForEach[offset+8*k+7]) {
              bool repeatEntry = true;
              for(uint8_t j=0; j<7; j++)
                if(newBest10ForEach[i-7+j] >> 8 != best10ForEach[offset+8*k+j] >> 8) {
                  repeatEntry = false;
                  break;
                }
              replaceIndex = repeatEntry ? 10 : k+1;
              if(repeatEntry)
                break;
            }
            else if(newBest10ForEach[i] < best10ForEach[offset+8*k+7]) {
              replaceIndex = k+1;
              break;
            }
  
          if(replaceIndex != 10) {
            for(uint8_t k=79; k >= 8*replaceIndex+8; k--)
              best10ForEach[offset+k] = best10ForEach[offset+k-8];
            for(uint8_t k=0; k<7; k++)
              best10ForEach[offset+8*replaceIndex + k] = newBest10ForEach[i-7+k];
            best10ForEach[offset+8*replaceIndex + 7] = newBest10ForEach[i];
          }
          replaceIndex = 0;
        }
      }
    }
  }
}

// back up the old file and create a new file with the results
void writeToFile(int16_t* best10, int16_t* worst10, int16_t* best10ForEach, uint8_t* combination=NULL, int32_t threadId=-1) {
  // back up old file if existent
  if(threadId == -1) {
    if(filesystem::exists(basePath/"fantasy_realms.data"))
      filesystem::rename(basePath/"fantasy_realms.data", basePath/"fantasy_realms_backup.data");
  } else
    if(filesystem::exists(basePath/format("fantasy_realms_{}.data", threadId)))
      filesystem::rename(basePath/format("fantasy_realms_{}.data", threadId), basePath/format("fantasy_realms_{}_backup.data", threadId));

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
    buffer[i] = combination == NULL ? static_cast<char>(_N-_K+i) : combination[i];
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
  if(threadId == -1)
    f.open(basePath/"fantasy_realms.data", ios_base::out | ios::binary); // no need to truncate because the old file was moved
  else
    f.open(basePath/format("fantasy_realms_{}.data", threadId), ios_base::out | ios::binary);
  f.write(buffer, 8647);
}

void forCombinationsDo(void (*hand_fn)(uint8_t*, int16_t*, int16_t*, int16_t*, int32_t),
    uint8_t* startCombination, int16_t* best10, int16_t* worst10, int16_t* best10ForEach) {
  uint8_t combination[_K];
  for(uint8_t i=0; i<_K; i++)
    combination[i] = startCombination[i];
  hand_fn(combination, best10, worst10, best10ForEach, -1);
  uint8_t i = 0;
  while(true) {
    if(i==_K-1 || combination[i]+1 != combination[i+1]) {
      combination[i]++;
      if(combination[i] == _N)
        break;
      i=0;
      hand_fn(combination, best10, worst10, best10ForEach, -1);
    } else {
      combination[i] = i;
      i++;
    }
  }
}

bool increaseCombination(uint8_t* combination) {
  uint8_t i = 0;
  while(true) {
    if(combination[i]+1 != combination[i+1]) {
      combination[i]++;
      return i!=_K-1 || combination[i]!=_N; // valid combination
    } else {
      combination[i] = i;
      i++;
    }
  }
}

// binomial coefficient
uint32_t binC(uint8_t n, uint8_t k) {
  uint8_t numerator = n, denominator = 1;
  uint32_t product = 1;
  for(uint8_t i=0; i<k; i++) {
    product *= numerator-i;
    product /= denominator+i;
  }
  return product;
}

void increaseCombination(uint8_t* arr, uint32_t diff) {
  uint8_t k=0;
  uint32_t val;
  while(true) {
    for(; k<7; k++) {
      val = binC(arr[k+1], k+1) - binC(arr[k]+1, k+1);
      if(val <= diff) {
        diff -= val;
      } else {
        for(uint8_t j=1; j < arr[k+1]-arr[k]-1; j++) { // this is a linear search; binary search might be faster
          val = binC(arr[k+1]-j, k+1) - binC(arr[k]+1, k+1);
          if(val <= diff) {
            diff -= val;
            arr[k] = arr[k+1]-1-j;
            break;
          }
        }
        break;
      }
    }
    if(diff > 0) {
      diff--;
      arr[k]++;
      for(uint8_t i=0; i<k; i++)
        arr[i] = i;
      k--;
    }
    if(diff == 0) {
      for(uint8_t i=k; i>0; i--)
        arr[i-1] = arr[i]-1;
      return;
    }
  }
}

bool increaseSelection(uint8_t* selection, uint8_t* limits) {
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

unsigned long combinationCounter = 0;
/*
  calculate the value of the combination
  store the combination + value in an ordered list of the best 10 values for each card type, ordered by total value descending
  store the list to memory
*/
void calculate(uint8_t* combination, int16_t* best10, int16_t* worst10, int16_t* best10ForEach, int32_t threadId=-1) {
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
  for(bool isValidSelection = true; isValidSelection; isValidSelection = increaseSelection(selection, selectionLimits)) {
    //reset cards for every selection
    for(uint8_t i=0; i<_K; i++) {
      hand[i] = cards[combination[i]]; // copy card
    }
    for(uint8_t i=0; i<_K; i++)
      if(hand[i].effects.hasMultipleCombinations)
        hand[i].effects.combinationCount = selection[i];

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

    // update cards
    uint8_t replaceIndex = 0;

    for(int8_t i=9; i>=0; i--)
      if(totalValue == best10[8*i+7]) {
        bool repeatEntry = true;
        for(uint8_t j=0; j<7; j++)
          if(hand[j].index != best10[8*i+j] >> 8) {
            repeatEntry = false;
            break;
          }
        replaceIndex = repeatEntry ? 10 : i+1;
        if(repeatEntry)
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
        if(repeatEntry)
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
          if(repeatEntry)
            break;
        }
        else if(totalValue < best10ForEach[offset+8*i+7]) {
          replaceIndex = i+1;
          break;
        }
      
      if(replaceIndex != 10) {
        for(uint8_t i=79; i >= 8*replaceIndex+8; i--)
          best10ForEach[offset + i] = best10ForEach[offset + i-8];
        for(uint8_t i=0; i<7; i++)
          best10ForEach[offset + 8*replaceIndex + i] = hand[i].index << 8 | hand[i].effects.combinationCount;
        best10ForEach[offset + 8*replaceIndex + 7] = totalValue;
      }
      replaceIndex = 0;
    }

    // increase combination counter and print progress
    if(threadId == -1) {
      combinationCounter++;
      if((combinationCounter & 0xffffff) == 0) { // equivalent to cC % 16'777'216 == 0 (16'777'216 == 2**24)
        // periodically back up the old file and create a new file with intermediate results
        writeToFile(best10, worst10, best10ForEach, combination);
          println("{}", combinationCounter >> 24); // print after writing to file!
      }
    }
  }
}

class ThreadPool {
  private:
    void threadLoop(uint32_t threadId);

    uint32_t numberOfThreads;
    uint32_t chunkSize;
    vector<thread> threads;
    mutex poolMutex;
    uint8_t nextCombination[_K+1] = {0, 1, 2, 3, 4, 5, 6, _N+1};
  
  public:
    ThreadPool(uint32_t n) : numberOfThreads(n), chunkSize(16'384*n) {}
    void run();
};

mutex fileMutex;
uint16_t fileUpdateCounter = 0;
void ThreadPool::threadLoop(uint32_t threadId) {
  int16_t best10[10*8] = {};
  int16_t worst10[10*8] = {};
  for(uint8_t i=7; i<80; i+=8)
    worst10[i] = 100;
  int16_t best10ForEach[10*8*52] = {};
  uint8_t currentCombination[_K+1];
  currentCombination[_K] = _N+1;

  readFromFile(best10, worst10, best10ForEach);

  uint16_t iterationCounter = 0;
  while(true) {
    {
      lock_guard<mutex> lg(poolMutex);
      for(uint8_t i=0; i<_K; i++)
        currentCombination[i] = nextCombination[i];
      if(currentCombination[_K-1] == _N) {
        println("Thread {} done!", threadId);
        return;
      }
      increaseCombination(nextCombination, chunkSize);
    }
    unsigned long combinationCounter = 0;
    do calculate(currentCombination, best10, worst10, best10ForEach, threadId);
    while(++combinationCounter < chunkSize && increaseCombination(currentCombination));
    // write results to file
    {
      lock_guard<mutex> lg(fileMutex);
      println("Updating file with thread {} data packet {}... Total updates: {}", threadId, ++iterationCounter, ++fileUpdateCounter);
      readFromFile(best10, worst10, best10ForEach);
      writeToFile(best10, worst10, best10ForEach, currentCombination);
    }
  }
}

void ThreadPool::run() {
  println("Adding threads to pool...");
  for(uint32_t i=0; i<numberOfThreads; i++) 
    threads.emplace_back(thread(&ThreadPool::threadLoop, this, i));
  println("Calculating...");
  for(uint32_t i=0; i<numberOfThreads; i++)
    threads[i].join();
  threads.clear();
}

int main(int argc, char** argv) {
  println("Getting file path...");
  filesystem::path filePath(argv[0]);
  basePath = filePath.remove_filename();

  uint16_t numberOfThreads = 1;
  println("Checking command line arguments...");
  if(argc > 1 && strcmp(argv[1], "-t") == 0) {
    if(argc == 2) {
      numberOfThreads = thread::hardware_concurrency();
      println("No number of threads specified. Using default of {}.", numberOfThreads);
    }
    else {
      numberOfThreads = atoi(argv[2]);
      if(numberOfThreads <= 0) {
        println("Number of threads must be positive.");
        return EXIT_FAILURE;
      }
    }
  }

  println("Initializing cards...");
  initializeCards();

  println("Initializing calculation...");
  for(uint8_t i=7; i<80; i+=8) {
    worst10[i] = 100; // initialize to a value somewhat above 0 to make sure positive values are recorded, 
    // otherwise they would be discarded because they weren't lower than the starting value
  }

  uint8_t startCombinationIndices[8] = {0, 1, 2, 3, 4, 5, 6, _N+1}; // set index 7 to _N+1 to more easily advance combinations in function 'increaseCombination(uint8_t*, uint32_t)'
  if(numberOfThreads == 1) {
    // check file integrity
    println("Checking if previous calculations exist...");
    readFromFile(best10, worst10, best10ForEach, startCombinationIndices);

    println("Calculating...");
    forCombinationsDo(calculate, startCombinationIndices, best10, worst10, best10ForEach);

    writeToFile(best10, worst10, best10ForEach);
  } else { // numberOfThreads > 1
    ThreadPool tp(numberOfThreads);
    tp.run();
  }

  println("Done!");

  return EXIT_SUCCESS;
}