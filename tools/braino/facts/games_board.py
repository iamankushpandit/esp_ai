"""Board and puzzle games: chess, go, backgammon, ludo, tic tac toe,
battleship, mazes, memory, sliding puzzle.

Every topic in this file was a total blank. The old extractor mined quiz
lookup tables out of the Gume C++ source, so a game that had no table produced
no facts at all - and none of these nine have one. Sixteen of the forty games
contributed zero words to training, which is why the model has never been able
to answer "how does a knight move" despite Chess being one of the games.

Chess rules are stated conservatively. Where a rule has fiddly exceptions
(en passant, the fifty move rule, underpromotion) it is left out rather than
stated half right - a five year old does not need it, and a wrong rule is
worse than a missing one.
"""
from . import Fact, report


def F(topic, answer, train, evalq):
    return Fact(topic=topic, train=list(train), evalq=list(evalq), answer=answer)


CHESS = "chess"
GO = "go"
BACK = "backgammon"
LUDO = "ludo"
TTT = "tic tac toe"
BATTLE = "battleship"
MAZE = "mazes"
MEM = "memory game"
SLIDE = "sliding puzzle"


# ---------------------------------------------------------------- chess
# (piece, how it moves) - the single most asked group of questions.
_PIECES = [
    ("pawn",
     "A pawn moves one square straight forward, or two on its very first move. "
     "It captures differently, by moving one square diagonally forward."),
    ("rook",
     "A rook moves in straight lines, as far as you like along a row or a "
     "column. It cannot move diagonally."),
    ("bishop",
     "A bishop moves diagonally, as far as you like. Each bishop stays on the "
     "same color square for the whole game."),
    ("knight",
     "A knight moves in an L shape, two squares one way and then one square "
     "across. It is the only piece that can jump over other pieces."),
    ("queen",
     "The queen moves in straight lines and diagonally, as far as you like. "
     "She is the most powerful piece on the board."),
    ("king",
     "The king moves one square at a time, in any direction. He is the most "
     "important piece, so he must always be kept safe."),
]

# (piece, count word, display) - how many each player starts with.
_COUNTS = [
    ("pawns", "eight", "Each player starts with eight pawns."),
    ("rooks", "two", "Each player starts with two rooks."),
    ("knights", "two", "Each player starts with two knights."),
    ("bishops", "two", "Each player starts with two bishops."),
    ("queens", "one", "Each player starts with one queen."),
    ("kings", "one", "Each player has one king."),
]

_CHESS = [
    ("Chess board",
     "A chess board has sixty-four squares, in eight rows of eight. The squares "
     "are light and dark, one after the other.",
     ["how many squares are on a chess board", "what does a chess board look like",
      "how big is a chess board", "tell me about the chess board"],
     ["how many squares does a chess board have"]),
    ("Pieces each",
     "Each player starts with sixteen pieces. That is eight pawns, two rooks, "
     "two knights, two bishops, one queen and one king.",
     ["how many chess pieces does each player have",
      "what pieces do you start with in chess",
      "how many pieces are there in chess", "what is in a chess set"],
     ["how many pieces does a chess player begin with"]),
    ("Aim of chess",
     "The aim of chess is to trap the other player's king so it cannot get "
     "away. That is called checkmate, and it wins the game.",
     ["how do you win at chess", "what is the aim of chess",
      "what are you trying to do in chess", "how does chess end"],
     ["what do you need to do to win a chess game"]),
    ("Check",
     "Check means the king is in danger and could be captured next turn. You "
     "must get out of check straight away.",
     ["what does check mean in chess", "what is check",
      "what happens when your king is in check", "what do you do in check"],
     ["can you explain check in chess"]),
    ("Checkmate",
     "Checkmate is when the king is in danger and there is no way out at all. "
     "The game ends there and that player has lost.",
     ["what is checkmate", "what does checkmate mean",
      "what is the difference between check and checkmate",
      "when does a chess game end"],
     ["can you explain checkmate"]),
    ("Who goes first",
     "White always moves first in chess, then the players take turns.",
     ["who goes first in chess", "which color starts in chess",
      "does white or black go first", "who moves first"],
     ["which player takes the first turn in chess"]),
    ("Stalemate",
     "Stalemate is when a player cannot make any legal move but their king is "
     "not in check. Nobody wins, so the game is a draw.",
     ["what is stalemate", "what does stalemate mean",
      "what happens if you cannot move in chess", "can a chess game be a draw"],
     ["can you explain stalemate"]),
    ("Capturing",
     "You capture by moving your piece onto a square where an enemy piece is "
     "standing. That piece comes off the board.",
     ["how do you capture in chess", "how do you take a piece",
      "what does capturing mean in chess", "how do pieces come off the board"],
     ["how do you take an enemy piece in chess"]),
    ("Castling",
     "Castling is a special move where the king slides two squares toward a "
     "rook and the rook hops over to the other side of him. It helps keep the "
     "king safe.",
     ["what is castling", "what does castling do", "how do you castle in chess",
      "what is the special king move"],
     ["can you explain castling"]),
    ("Pawn promotion",
     "If a pawn reaches the far end of the board, it turns into another piece. "
     "Most players choose a queen, because she is the strongest.",
     ["what happens when a pawn reaches the end",
      "what is pawn promotion", "can a pawn become a queen",
      "what happens at the other side of the board"],
     ["what does a pawn turn into"]),
    ("Strongest piece",
     "The queen is the strongest piece, because she can move in straight lines "
     "and diagonally as far as she likes.",
     ["what is the strongest piece in chess", "which chess piece is best",
      "what is the most powerful chess piece", "which piece can move the most"],
     ["which chess piece is the most powerful"]),
    ("Most important piece",
     "The king is the most important piece. You can lose every other piece and "
     "keep playing, but if the king is trapped the game is over.",
     ["what is the most important piece in chess",
      "which piece matters most in chess", "why is the king important",
      "what piece must you protect"],
     ["which chess piece matters the most"]),
    ("Piece that jumps",
     "The knight is the only piece that can jump over others. Everything else "
     "has to go around.",
     ["which chess piece can jump", "can any piece jump over others",
      "what piece jumps in chess", "which piece does not have to go around"],
     ["which chess piece is allowed to jump"]),
    ("Where chess came from",
     "Chess is very old. It began in India a very long time ago and slowly "
     "spread all around the world.",
     ["where did chess come from", "how old is chess", "who invented chess",
      "where was chess invented"],
     ["what country did chess start in"]),
    ("Chess is a thinking game",
     "Chess has no dice and no luck at all. Every move is a choice, so the "
     "player who thinks ahead best usually wins.",
     ["is chess about luck", "is there luck in chess",
      "why is chess hard", "what makes someone good at chess"],
     ["does luck matter in chess"]),
]

# ---------------------------------------------------------------- others
_OTHER = [
    (GO, "What go is",
     "Go is a board game for two players. You take turns putting stones on the "
     "lines of a grid, and you try to surround more space than your opponent.",
     ["what is the game of go", "how do you play go", "tell me about go",
      "what is go"],
     ["can you explain the game of go"]),
    (GO, "Go stones",
     "In go, one player uses black stones and the other uses white. You place "
     "them on the crossing points of the lines, not inside the squares.",
     ["what do you use to play go", "what are go stones",
      "where do you put the stones in go", "what colors are go stones"],
     ["how do the pieces work in go"]),
    (GO, "Go is old",
     "Go is one of the oldest board games in the world. People have been "
     "playing it in China for thousands of years.",
     ["how old is go", "where did go come from", "is go an old game",
      "where was go invented"],
     ["what country did go start in"]),
    (GO, "Go and chess",
     "In chess you capture the other pieces, but in go you surround empty "
     "space. Go has a bigger board and simpler rules.",
     ["how is go different from chess", "what is the difference between go and chess",
      "is go like chess", "which is harder go or chess"],
     ["how do go and chess differ"]),

    (BACK, "What backgammon is",
     "Backgammon is a race game for two players. You roll dice and move your "
     "checkers around the board, trying to get them all home first.",
     ["what is backgammon", "how do you play backgammon",
      "tell me about backgammon", "what is the aim of backgammon"],
     ["can you explain backgammon"]),
    (BACK, "Backgammon checkers",
     "Each player has fifteen checkers in backgammon, and moves them around the "
     "board according to the dice.",
     ["how many pieces are in backgammon", "how many checkers do you have",
      "what do you move in backgammon", "how many counters in backgammon"],
     ["how many checkers does each backgammon player have"]),
    (BACK, "Luck and skill",
     "Backgammon mixes luck and skill. The dice decide how far you can move, "
     "but you choose which checkers to move, and that choice matters.",
     ["is backgammon luck or skill", "does backgammon need skill",
      "is backgammon just about dice", "what makes someone good at backgammon"],
     ["how much of backgammon is luck"]),

    (LUDO, "What ludo is",
     "Ludo is a race game for up to four players. You roll a die and move your "
     "four tokens around the board, trying to get them all home first.",
     ["what is ludo", "how do you play ludo", "tell me about ludo",
      "how do you win ludo"],
     ["can you explain the game of ludo"]),
    (LUDO, "Ludo tokens",
     "Each player has four tokens in ludo, all the same color. You have to get "
     "every one of them home to win.",
     ["how many pieces do you have in ludo", "how many tokens in ludo",
      "what do you move in ludo", "how many counters does each player get"],
     ["how many tokens does a ludo player have"]),
    (LUDO, "Taking turns",
     "In ludo everyone takes turns rolling the die. Waiting for your turn is "
     "part of the game, and it is what makes it fair.",
     ["how do turns work in ludo", "who goes first in ludo",
      "why do you take turns", "what happens on your turn in ludo"],
     ["how does taking turns work in ludo"]),

    (TTT, "What tic tac toe is",
     "Tic tac toe is played on a grid of nine squares. One player is X and the "
     "other is O, and you take turns filling in squares.",
     ["what is tic tac toe", "how do you play tic tac toe",
      "how many squares in tic tac toe", "tell me about tic tac toe"],
     ["can you explain tic tac toe"]),
    (TTT, "Winning tic tac toe",
     "You win tic tac toe by getting three of your marks in a row. The row can "
     "go across, down, or corner to corner.",
     ["how do you win tic tac toe", "what do you need to win at tic tac toe",
      "how many in a row do you need", "what counts as a win in tic tac toe"],
     ["what do you have to do to win tic tac toe"]),
    (TTT, "Tic tac toe draws",
     "Tic tac toe often ends in a draw. If both players play carefully, nobody "
     "can win, and all nine squares get filled.",
     ["why does tic tac toe end in a draw", "can nobody win tic tac toe",
      "why is tic tac toe always a tie", "what is a draw in tic tac toe"],
     ["why do tic tac toe games tie so often"]),
    (TTT, "Tic tac toe strategy",
     "The middle square is the best one to take first, because more winning "
     "rows go through it than any other square.",
     ["what is the best first move in tic tac toe",
      "how do you get good at tic tac toe", "where should i go first",
      "what is a good tic tac toe move"],
     ["which square should you take first"]),

    (BATTLE, "What battleship is",
     "Battleship is a guessing game. Each player hides ships on a grid, and you "
     "take turns guessing squares to try to find them.",
     ["what is battleship", "how do you play battleship",
      "tell me about battleship", "what is the sea battle game"],
     ["can you explain battleship"]),
    (BATTLE, "Hits and misses",
     "If your guess lands on a ship it is a hit, and if it lands on empty water "
     "it is a miss. You win by finding all the other player's ships.",
     ["what is a hit in battleship", "how do you win battleship",
      "what happens when you guess right", "what is a miss in battleship"],
     ["how do you know if you hit a ship"]),
    (BATTLE, "Grid coordinates",
     "The squares on a battleship grid have a letter and a number, like B four. "
     "The letter tells you the row and the number tells you the column.",
     ["how do you name a square in battleship", "what are coordinates",
      "how do you say which square", "what does b four mean"],
     ["how do you describe a square on the grid"]),
    (BATTLE, "Battleship strategy",
     "Once you get a hit, guess the squares right next to it. Ships are in a "
     "straight line, so the rest of it must be beside the hit.",
     ["how do you get good at battleship", "what should i do after a hit",
      "what is a good battleship strategy", "where should i guess next"],
     ["what is the best move after you hit a ship"]),

    (MAZE, "What a maze is",
     "A maze is a puzzle of paths with walls between them. You start at one end "
     "and try to find the way through to the other.",
     ["what is a maze", "how do mazes work", "tell me about mazes",
      "what do you do in a maze"],
     ["can you explain what a maze is"]),
    (MAZE, "Solving a maze",
     "One easy trick is to keep one hand on the same wall and follow it. It is "
     "slow, but in most mazes it will walk you all the way out.",
     ["how do you solve a maze", "how do you get out of a maze",
      "what is a good way through a maze", "is there a trick for mazes"],
     ["what is the best way to escape a maze"]),
    (MAZE, "Dead end",
     "A dead end is a path that stops and goes nowhere. When you reach one, go "
     "back and try a different turning.",
     ["what is a dead end", "what do you do at a dead end",
      "what does dead end mean", "what if the path stops"],
     ["what should you do when a path ends"]),
    (MAZE, "Labyrinth",
     "A labyrinth is like a maze but with only one path. You cannot get lost, "
     "because there are no choices to make.",
     ["what is a labyrinth", "how is a labyrinth different from a maze",
      "can you get lost in a labyrinth", "tell me about labyrinths"],
     ["what makes a labyrinth different from a maze"]),

    (MEM, "Memory game",
     "In a memory game all the cards are face down. You turn two over at a "
     "time, and if they match you keep them.",
     ["what is a memory game", "how do you play the memory game",
      "how does the matching game work", "what do you do in a memory game"],
     ["can you explain the memory game"]),
    (MEM, "Memory tips",
     "The trick is to remember where the cards are, even the ones that did not "
     "match. Saying the place out loud in your head really helps.",
     ["how do you get better at the memory game", "how can i remember the cards",
      "what is a good memory tip", "how do i win at memory"],
     ["what helps you remember where cards are"]),
    (MEM, "What memory is",
     "Memory is how your brain keeps things it has learned so you can use them "
     "later. Practicing something makes the memory stronger.",
     ["what is memory", "how does memory work", "what does my brain remember with",
      "how do i remember things"],
     ["can you explain how memory works"]),
    (MEM, "A pair",
     "A pair is two things that go together, like two matching cards, two socks "
     "or two shoes.",
     ["what is a pair", "what does pair mean", "how many is a pair",
      "what makes a pair"],
     ["can you explain what a pair is"]),

    (SLIDE, "Sliding puzzle",
     "A sliding puzzle has tiles in a frame with one empty space. You slide "
     "tiles into the gap, one at a time, to get them into the right order.",
     ["what is a sliding puzzle", "how does a sliding puzzle work",
      "how do you play a sliding puzzle", "tell me about sliding puzzles"],
     ["can you explain the sliding puzzle"]),
    (SLIDE, "Why the gap matters",
     "The empty space is what makes the puzzle work. Without a gap there would "
     "be nowhere for a tile to move.",
     ["why is there an empty space in a sliding puzzle",
      "what is the gap for", "why is one square empty",
      "what happens without the empty space"],
     ["why does the sliding puzzle need a gap"]),
    (SLIDE, "Sliding puzzle tips",
     "Finish one row at a time, starting at the top. Once a row is right, try "
     "not to disturb it while you work on the next one.",
     ["how do you solve a sliding puzzle", "what is a good way to do a sliding puzzle",
      "how do i get the tiles in order", "any tips for sliding puzzles"],
     ["what is the best way to finish a sliding puzzle"]),
]


def build():
    facts = []

    for _name, answer, train, evalq in _CHESS:
        facts.append(F(CHESS, answer, train, evalq))
    for topic, _name, answer, train, evalq in _OTHER:
        facts.append(F(topic, answer, train, evalq))

    # Piece moves: the most asked chess questions, so they get the most
    # phrasings. Generated so all six read identically in structure.
    for piece, answer in _PIECES:
        facts.append(F(CHESS, answer,
                       [f"how does a {piece} move",
                        f"how does the {piece} move in chess",
                        f"what way does a {piece} go",
                        f"tell me how the {piece} moves"],
                       [f"how do you move a {piece} in chess"]))

    for plural, _count, answer in _COUNTS:
        facts.append(F(CHESS, answer,
                       [f"how many {plural} do you start with",
                        f"how many {plural} are there in chess",
                        f"number of {plural} in chess",
                        f"how many {plural} does each player get"],
                       [f"how many {plural} does a player begin with"]))

    return facts


if __name__ == "__main__":
    from collections import Counter
    f = build()
    report("games_board", f)
    for t, n in Counter(x.topic for x in f).most_common():
        print(f"   {t:16s} {n}")
