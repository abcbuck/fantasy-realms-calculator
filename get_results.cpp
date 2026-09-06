#include <fstream>
#include <print>
#include <string>
using namespace std;

int main() {
  string names[52] = {
    "Gestaltwandler", // 0
    "Spiegelung",
    "Doppelgänger",
    "Buch der Veränderung",
    "Rune des Schutzes",
    "Insel", // 5
    "Herr der Bestien",
    "Gebirge",
    "Höhle",
    "Waldläufer",
    "Kriegsschiff", // 10
    "Große Flut",
    "Basilisk",
    "Buschfeuer",
    "Blizzard",
    "Rauch", // 15
    "Kampfzeppelin",
    "Regensturm",
    "Hexenmeister",
    "Sumpf",
    "Drache", // 20
    "Kaiserin",
    "Zwergeninfanterie",
    "Leichte Kavallerie",
    "Ritter",
    "Magierin", // 25
    "Sammler",
    "Quelle des Lebens",
    "Wasserwesen",
    "Schlachtross",
    "Einhorn", // 30
    "Hydra",
    "Kerze",
    "Feuerwesen",
    "Schmiede",
    "Blitz", // 35
    "Weltenbaum",
    "Schild von Keth",
    "Juwel der Ordnung",
    "Prinzessin",
    "Kriegsherr", // 40
    "Königin",
    "König",
    "Erdwesen",
    "Wald",
    "Glockenturm", // 45
    "Luftwesen",
    "Wirbelsturm",
    "Elbenschützen",
    "Zauberstab",
    "Elbischer Bogen", // 50
    "Schwert von Keth"
  };

  int best10[10*8] = {}; // 8 = 7 cards + 1 value
  int worst10[10*8] = {};

  for(int i=7; i<80; i+=8) {
    worst10[i] = 100; // initialize to a value somewhat above 0 to make sure positive values are recorded, 
    // otherwise they would be discarded because they weren't lower than the starting value
  }

  int best10ForEach[10*8*52] = {};

  fstream f;
  f.open("fantasy_realms_combinations.data", ios_base::in | ios::binary);

  char cardData[13];
  f.read(cardData, 13);
  
  int cards[8];

  int cardIndex0 = cardData[0] >> 2;
  int cardCombination0 = (cardData[0] & 0x3) << 5 | cardData[1] >> 3;
  int cards[0] = 256*cardIndex0 + cardCombination0;

  int cardIndex1 = (cardData[1] & 0x7) << 3 | cardData[2] >> 5;
  int cardCombination1 = (cardData[2] & 0x1f) << 2 | cardData[3] >> 6;
  int cards[1] = 256*cardIndex1 + cardCombination1;

  int cardIndex2 = cardData[3] & 0x3f;
  int cardCombination2 = cardData[4] >> 1;
  int cards[2] = 256*cardIndex2 + cardCombination2;

  int cardIndex3 = (cardData[4] & 0x1) << 5 | cardData[5] >> 3;
  int cardCombination3 = (cardData[5] & 0x7) << 4 | cardData[6] >> 4;
  int cards[3] = 256*cardIndex3 + cardCombination3;

  int cardIndex4 = (cardData[6] & 0xf) << 2 | cardData[7] >> 6;
  int cardCombination4 = (cardData[7] & 0x3f) << 1 | cardData[8] >> 7;
  int cards[4] = 256*cardIndex4 + cardCombination4;

  int cardIndex5 = (cardData[8] & 0x7e) >> 1;
  int cardCombination5 = (cardData[8] & 0x1) << 6 | cardData[9] >> 2;
  int cards[5] = 256*cardIndex5 + cardCombination5;

  int cardIndex6 = (cardData[9] & 0x3) << 4 | cardData[10] >> 4;
  int cardCombination6 = (cardData[10] & 0xf) << 3 | cardData[11] >> 5;
  int cards[6] = 256*cardIndex6 + cardCombination6;

  int cards[7] = ((cardData[11] >> 4) & 0x1 ? -1 : 1) * (static_cast<int>(cardData[11] & 0xf) << 8 | cardData[12]); // value

  // update cards
  int replaceIndex = -1;

  // update best cards
  for(int i=0; i<10; i++)
    if(cards[7] > best10[8*i+7]) {
      replaceIndex = i;
      break;
    }
  
  if(replaceIndex != -1) {
    for(int i=79; i >= replaceIndex+8; i--)
      best10[i] = best10[i-8];
    for(int i=0; i<8; i++)
      best10[8*replaceIndex + i] = cards[i];

    replaceIndex = -1;
  }

  // update worst cards
  for(int i=0; i<10; i++)
    if(cards[7] < worst10[8*i+7]) {
      replaceIndex = i;
      break;
    }
  
  if(replaceIndex != -1) {
    for(int i=79; i >= replaceIndex+8; i--)
      worst10[i] = worst10[i-8];
    for(int i=0; i<8; i++)
      worst10[8*replaceIndex + i] = cards[i];

    replaceIndex = -1;
  }

  // update best cards per type
  for(int j=0; j<7; j++) {
    int offset = 80*(cards[j]/256);
    for(int i=0; i<10; i++)
      if(cards[7] > best10ForEach[offset+8*i+7]) {
        replaceIndex = i;
        break;
      }
    
    if(replaceIndex != -1) {
      for(int i=offset + 79; i >= offset + replaceIndex+8; i--)
        best10ForEach[offset + i] = best10ForEach[offset + i-8];
      for(int i=0; i<8; i++)
        best10ForEach[offset + 8*replaceIndex + i] = cards[i];

      replaceIndex = -1;
    }
  }

  f.close();

  // print results
  println("Best cards:");
  #error Calculation stopped! Fix problem there first.

  return 0;
}