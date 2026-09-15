#include <filesystem>
#include <fstream>
#include <print>
#include <string>
using namespace std;

int main(int argc, char** argv) {
  filesystem::path basePath = filesystem::path(argv[0]).remove_filename();

  string names_german[52] = {
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

  string names[52] = {
    "Doppelgänger", // 0
    "Mirage",
    "Shapeshifter",
    "Book of Changes",
    "Protection Rune",
    "World Tree", // 5
    "Shield of Keth",
    "Gem of Order",
    "Enchantress",
    "Collector",
    "Beastmaster", // 10
    "Warlock Lord",
    "Air Elemental",
    "Rainstorm",
    "Whirlwind",
    "Smoke", // 15
    "Blizzard",
    "Fountain of Life",
    "Water Elemental",
    "Island",
    "Swamp", // 20
    "Great Flood",
    "Candle",
    "Fire Elemental",
    "Forge",
    "Lightning", // 25
    "Wildfire",
    "Warhorse",
    "Unicorn",
    "Hydra",
    "Dragon", // 30
    "Basilisk",
    "Magic Wand",
    "Elven Longbow",
    "Sword of Keth",
    "Warship", // 35
    "War Dirigible",
    "Princess",
    "Warlord",
    "Queen",
    "King", // 40
    "Empress",
    "Earth Elemental",
    "Cavern",
    "Forest",
    "Bell Tower", // 45
    "Mountain",
    "Rangers",
    "Elven Archers",
    "Dwarvish Infantry",
    "Light Cavalry", // 50
    "Knights"
  };

  int best10[10*8] = {}; // 8 = 7 cards + 1 value
  int worst10[10*8] = {};

  for(int i=7; i<80; i+=8) {
    worst10[i] = 100; // initialize to a value somewhat above 0 to make sure positive values are recorded, 
    // otherwise they would be discarded because they weren't lower than the starting value
  }

  int best10ForEach[10*8*52] = {};

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