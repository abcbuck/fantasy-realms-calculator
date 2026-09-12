#include <filesystem>
#include <fstream>
#include <print>
#include <string>
using namespace std;

int main(int argc, char** argv) {
  filesystem::path basePath = filesystem::path(argv[0]).remove_filename();

  string names[52] = {
    "Doppelgänger", // 0
    "Spiegelung",
    "Gestaltwandler",
    "Buch der Veränderung",
    "Rune des Schutzes",
    "Weltenbaum", // 5
    "Schild von Keth",
    "Juwel der Ordnung",
    "Magierin",
    "Sammler",
    "Herr der Bestien", // 10
    "Hexenmeister",
    "Luftwesen",
    "Regensturm",
    "Wirbelsturm",
    "Rauch", // 15
    "Blizzard",
    "Quelle des Lebens",
    "Wasserwesen",
    "Insel",
    "Sumpf", // 20
    "Große Flut",
    "Kerze",
    "Feuerwesen",
    "Schmiede",
    "Blitz", // 25
    "Buschfeuer",
    "Schlachtross",
    "Einhorn",
    "Hydra",
    "Drache", // 30
    "Basilisk",
    "Zauberstab",
    "Elbischer Bogen",
    "Schwert von Keth",
    "Kriegsschiff", // 35
    "Kampfzeppelin",
    "Prinzessin",
    "Kriegsherr",
    "Königin",
    "König", // 40
    "Kaiserin",
    "Erdwesen",
    "Höhle",
    "Wald",
    "Glockenturm", // 45
    "Gebirge",
    "Waldläufer",
    "Elbenschützen",
    "Zwergeninfanterie",
    "Leichte Kavallerie", // 50
    "Ritter"
  };

  int best10[10*8] = {}; // 8 = 7 cards + 1 value
  int worst10[10*8] = {};

  for(int i=7; i<80; i+=8) {
    worst10[i] = 100; // initialize to a value somewhat above 0 to make sure positive values are recorded, 
    // otherwise they would be discarded because they weren't lower than the starting value
  }

  int best10ForEach[10*8*52] = {};

  /* fstream f;
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
   */

  int startCombinationIndices[7] = {0, 1, 2, 3, 4, 5, 6};

  // check file integrity
  println("Checking if calculations exist...");
  bool calculationsExist = filesystem::exists(basePath/"fantasy_realms.data");

  if(calculationsExist) {
    // continue from file
    println("Loading data...");
    fstream f;
    f.open(basePath/"fantasy_realms.data", ios_base::in | ios_base::binary);
    char buffer[8647];
    f.read(buffer, 8647);
    if(f.gcount() != 8647) {
      println("Corrupted data. Please replace the file with a correctly formatted file or remove it. Terminating the program...");
      return EXIT_FAILURE;
    }
    f.close();
    
    for(int i=0; i<7; i++)
      startCombinationIndices[i] = buffer[i];
    int offset = 7;
    for(int i=0; i<80; i++) {
      if(i%8 != 7)
        best10[i] = (buffer[offset+2*i] << 8) + buffer[offset+2*i+1];
      else {
        best10[i] = buffer[offset+2*i] << 8 | static_cast<unsigned char>(buffer[offset+2*i+1]);
        /* bool negative = (buffer[offset+2*i] & 0x80) != 0; // not needed, the compiler does this automatically because it keeps the sign of a signed char on conversion
        if(negative)
          best10[i] |= 0xffff0000; */
      }
    }
    offset += 160;
    for(int i=0; i<80; i++) {
      if(i%8 != 7)
        worst10[i] = (buffer[offset+2*i] << 8) + buffer[offset+2*i+1];
      else {
        worst10[i] = buffer[offset+2*i] << 8 | static_cast<unsigned char>(buffer[offset+2*i+1]);
        /* bool negative = (buffer[offset+2*i] & 0x80) != 0;
        if(negative)
          worst10[i] |= 0xffff0000; */
      }
    }
    offset += 160;
    for(int i=0; i<52*80; i++) {
      if(i%8 != 7)
        best10ForEach[i] = (buffer[offset+2*i] << 8) + buffer[offset+2*i+1];
      else {
        best10ForEach[i] = buffer[offset+2*i] << 8 | static_cast<unsigned char>(buffer[offset+2*i+1]);
        /* bool negative = (buffer[offset+2*i] & 0x80) != 0;
        if(negative)
          best10ForEach[i] |= 0xffff0000; */
      }
    }
  } else {
    println("No data file found.");
    return 0;
  }
  
/* 
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
      for(int i=79; i >= replaceIndex+8; i--)
        best10ForEach[offset + i] = best10ForEach[offset + i-8];
      for(int i=0; i<8; i++)
        best10ForEach[offset + 8*replaceIndex + i] = cards[i];

      replaceIndex = -1;
    }
  } */

  println("Last combination values found in file: {}, {}, {}, {}, {}, {}, {}",
    startCombinationIndices[0],
    startCombinationIndices[1],
    startCombinationIndices[2],
    startCombinationIndices[3],
    startCombinationIndices[4],
    startCombinationIndices[5],
    startCombinationIndices[6]);

  // print results
  println("Printing the results...");
  // get the 10 best combinations + value
  println("\nBest combinations:");
  for(int i=0; i<10; i++)
    println("{}. {}, {}, {}, {}, {}, {}, {}; {} points", i+1,
      names[best10[8*i] >> 8],
      names[best10[8*i+1] >> 8],
      names[best10[8*i+2] >> 8],
      names[best10[8*i+3] >> 8],
      names[best10[8*i+4] >> 8],
      names[best10[8*i+5] >> 8],
      names[best10[8*i+6] >> 8], best10[8*i+7]);

  // get the 10 worst combinations + value
  println("\nWorst combinations:");
  for(int i=0; i<10; i++)
    println("{}. {}, {}, {}, {}, {}, {}, {}; {} points", i+1,
      names[worst10[8*i] >> 8],
      names[worst10[8*i+1] >> 8],
      names[worst10[8*i+2] >> 8],
      names[worst10[8*i+3] >> 8],
      names[worst10[8*i+4] >> 8],
      names[worst10[8*i+5] >> 8],
      names[worst10[8*i+6] >> 8], worst10[8*i+7]);
  println();

  // get the 10 best combinations for every card + value
  for(int j=0; j<52; j++) {
    println("\nBest combinations with {}:", names[j]);
    for(int i=0; i<10; i++)
      println("{}. {}, {}, {}, {}, {}, {}, {}; {} points", i+1,
        names[best10ForEach[80*j+8*i] >> 8],
        names[best10ForEach[80*j+8*i+1] >> 8],
        names[best10ForEach[80*j+8*i+2] >> 8],
        names[best10ForEach[80*j+8*i+3] >> 8],
        names[best10ForEach[80*j+8*i+4] >> 8],
        names[best10ForEach[80*j+8*i+5] >> 8],
        names[best10ForEach[80*j+8*i+6] >> 8], best10ForEach[80*j+8*i+7]);
  }

  return 0;
}