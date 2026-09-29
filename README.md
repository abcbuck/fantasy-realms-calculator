# General

This program calculates the card combinations with the ten highest scores, ten least scores and ten highest scores of every card for the card game Fantasy Realms, disregarding the Necromancer. The program runs by brute force with optional multithreading.

For a discussion of the algorithmic aspect of the program have a look at [my website](https://robinlbuck.org).

# Usage

Compile the files main.cpp and get_results.cpp. For compilation instructions see the bottom of this README.
main.cpp compiles to the main program that calculates the best scores and stores them to a file.
get_results.cpp compiles to a reader for the results.

You may run the main program with the `-t [number]` option to make it run with multiple threads. If no number is specified, it defaults to your system's supported number of threads.

If you run the program without the `-t` option, every few seconds, it stores intermediate results in the file 'fantasy_realms.data' and prints a number just after storing. This allows you to interrupt the program safely (by pressing Ctrl+C) just after a number has been printed and continue calculation later. When you run the program the next time, it will load the data and continue calculation from there. In case you're not sure if the data was saved correctly because you closed the program just when it printed a number or you closed the program without paying attention to the numbers, replace 'fantasy_realms.data' with 'fantasy_realms_backup.data', which contains the data of the last 'fantasy_realms.data' prior to the current one, to ensure you get valid results.

If you run the program with the `-t` option, it uses the file 'fantasy_realms.data' to sync data between threads. In this case, interrupting the program early may skip some card combinations, so in this case, I'd recommend to let the program run till its done if you want to get valid results.

get_results.cpp contains an array of card names in English and an array of card names in German for the convenience of German users. You may replace the English card name array with the German card name array to get the reader to display the results in German. Similarly, you may add the cards of any other language this way.

You may change the card combinations considered during calculation by removing some of the cards. The simplest way to do this is to set a card's `effects.invalid` property to `true`. For example to remove the Gem of Order from the results, look for 'Gem of Order' in the source code to find it has `cards` index 7, add the code `cards[7].effects.invalid = true;` to the respective section of the main() function, then compile.
Running the program with all cards takes 19 minutes on my 3.8 GHz 16 thread machine. If I remove the first four cards, it takes only 4 seconds. (Doppelgänger, Mirage, Shapeshifter and Book of Changes require the most processing.)

NOTE: The program uses the files 'fantasy_realms.data' and 'fantasy_realms_backup.data' to store results during calculation and when the calculation is done. When recalculating the results for a non-superset of a previous calculation, make sure to remove or rename 'fantasy_realms.data'. Else the previous results will be loaded and considered a partial result of the new results which leads to cards not part of the set of cards you want to consider showing up in the results.

NOTE: The Necromancer isn't part of the program for simplicity. Studying the results, you will quickly find him to be among the best combinations as, due to his effect, he may be added as an eighth card to most of the best combinations. He also synergizes well with the Gem of Order, which may allow for even stronger combinations than just adding him to the best combinations obtained without considering him as a card.
Someone thought about what the best combinations including the necromancer might be [here](https://boardgamegeek.com/thread/1870506/app-of-scoring).

# How to compile a C++ program

1. Get a compiler.
2. Use the compiler.

That's as simple as it can get. However, if you've never done it before, it can be tough to read through all the jargon on programming webpages to the point where you don't even know if you're headed in the right direction. So I'm going to expand on this two-step guide to give up its simplicity for the sake of quick results:

## Get a compiler

Download and install [LLVM](https://github.com/llvm/llvm-project/releases). LLVM is a compiler toolset, part of which is Clang++, the C++ compiler you'll be using. Unfortunately, they don't offer the compiler for download as a single file, so you'll have to install the whole project. Luckily, if you only need a basic C++ compiler, you'll never have to do it again.
I could provide a mirror for you to download the file without any more files you don't need to save bandwidth but _a._ it would quickly be outdated, and _b._ you'd have to trust me it isn't actually malware. Better download from the official site.

## Use the compiler

After installation(!), open a new terminal from the folder where you put the main.cpp and get_results.cpp, then run:
`clang++ -std=c++23 -o calculate_best_cards.exe -O3 main.cpp` and
`clang++ -std=c++23 -o get_results_en.exe -O3 get_results.cpp`.
`clang++` is the compiler you just installed. Options are specified with a dash (`-`).
`-std=<standard>` specifies the C++ standard to use. My code requires at least C++23. For a full list of options, see [the manual](https://releases.llvm.org/23.1.0/tools/clang/docs/CommandGuide/clang.html#cmdoption-std).
`-o` specifies the output file (the actual program you can run after compilation).
`-O3` lets the compiler optimize the program to make it faster.

## Run the program

Having compiled the program, now you can run it from your existing terminal!:
`./calculate_best_cards.exe -t`
And view the results when it's done!:
`./get_results_en.exe`

If you run the program with all cards and multithreading, it takes a minute to get the first output showing the program is at work. If you don't see any output for 10 minutes, there is a problem, your computer runs at significantly less than 1 GHz or you've specified way too many threads (I scale thread workload with the number of threads so that, on average, the output throughput is the same for the program across all thread counts; therefore the first output takes longer to appear for higher thread counts). The program is done when the update counter reaches 511 for 16 threads.

# Results

For the full results, use the reader on the \*.data files calculated by the main program. For convenience, I have provided some results in binary and text format.

## A short examination
The cards that consistently appear at the top of each card's best combinations list are the Gem of Order, the Candle and the Collector because of their great boni.
I calculated the best combinations to the full set of cards (without the necromancer) and to the sets without each of these cards and neither of them to get the next best results.
Roughly speaking, if you can't get the Gem of Order, Candle or Collector to work, the next best strategies seem to be get King and Queen and collect armies, followed by get Rainstorm and collect Floods and the Mountain-Smoke-Wildfire combo combined with the World Tree.
The worst cards to me seem to be the Protection Rune, Earth Elemental, the war vehicles, Beasts and the Empress. The Protection Rune's effect is never really worth it because there aren't so many cards with punishing effects that combine well with other cards when their punishment is lifted, a strategy combining cards with bonus effects only generally fares better. The Earth Elemental doesn't really make sense because it combines well with other Lands, which combine well with other cards of all different kinds, so there is no strategy that can both have many Lands and satisfy their effects. War Dirigible might be used to supplement Armies, and Warship to supplement Floods, though more Armies or Floods, respectively, are preferable. Beasts in a similar vein might best replace missing cards in other combinations, with respect to their effects. Finally, the Empress isn't that good of a card because her bonus isn't enough to make the by themselves rather mediocre-valued Armies good compared to other strategies. 
