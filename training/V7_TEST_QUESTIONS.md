# v7 — test questions for the implementer

Every question below was run through `model_8m_kid_v7.zip` on the host and
the answer shown is what it actually returned. **103 of 103 pass.**

## Read this first: ask the EXACT wording

v7 answers **100.0%** correctly on a wording it was trained on and **42.5%**
when the same fact is reworded — the widest gap of any version. Only 0.0% of
facts are wrong under every wording, so the knowledge is all there; what fails
is recognising a paraphrase.

A failure on your own phrasing is therefore expected behaviour, not a bad
flash. Use these strings to confirm the device works, then reword deliberately
if you want to see the gap for yourself.

Speech input gives lower case, no punctuation, numbers as words.
---

## Greetings and identity

| ask | v7 answers | |
|---|---|---|
| `hello` | Hello! It is lovely to hear from you. What would you like to know today? | ✅ |
| `hi` | Hello! It is lovely to hear from you. What would you like to know today? | ✅ |
| `what are you` | I am Braino! I am a little learning gadget. You talk to me and I answer your questions about words, numbers, animals, and the world. | ✅ |
| `what can you do` | I can answer questions, help you with spelling and numbers, explain what words mean, tell you jokes, and share facts about the world. | ✅ |
| `who made you` | I am not clever enough to answer that yet. I hope my makers teach me about that one day! | ✅ |
| `thank you` | You are very welcome! That is what you say back when somebody thanks you. | ✅ |
| `goodbye` | Goodbye! Come back and talk to me any time you like. | ✅ |

## Capitals and countries

| ask | v7 answers | |
|---|---|---|
| `what is the capital of france` | The capital of France is Paris. | ✅ |
| `what is the capital of japan` | The capital of Japan is Tokyo. | ✅ |
| `what is the capital of italy` | The capital of Italy is Rome. | ✅ |
| `what is the capital of egypt` | The capital of Egypt is Cairo. | ✅ |
| `what is the capital of brazil` | The capital of Brazil is Brasilia. | ✅ |
| `what continent is kenya in` | Kenya is in Africa. | ✅ |
| `what continent is brazil in` | Brazil is in South America. | ✅ |

## US states

| ask | v7 answers | |
|---|---|---|
| `what is the capital of texas` | The capital of Texas is Austin. | ✅ |
| `what is the capital of california` | The capital of California is Sacramento. | ✅ |
| `what is the capital of florida` | The capital of Florida is Tallahassee. | ✅ |
| `what is the capital of new york` | The capital of New York is Albany. | ✅ |

## Chess

| ask | v7 answers | |
|---|---|---|
| `how does a knight move in chess` | A knight moves in an L shape: two squares one way and one square to the side. It can jump over pieces. | ✅ |
| `how does a bishop move` | A bishop moves diagonally, as far as you like. Each bishop stays on the same color square for the whole game. | ✅ |
| `how does a rook move` | A rook moves in straight lines, as far as you like along a row or a column. It cannot move diagonally. | ✅ |
| `how does the queen move` | The queen moves in straight lines and diagonally, as far as you like. She is the most powerful piece on the board. | ✅ |
| `how many squares are on a chess board` | A chess board has sixty-four squares, in eight rows of eight. The squares are light and dark, one after the other. | ✅ |
| `how many pawns do you start with` | Each player starts with eight pawns. | ✅ |
| `what is checkmate` | Checkmate is when the king is in danger and there is no way out at all. The game ends there and that player has lost. | ✅ |
| `who goes first in chess` | White always moves first in chess, then the players take turns. | ✅ |

## Other games

| ask | v7 answers | |
|---|---|---|
| `what is the game of go` | Go is a board game for two players. You take turns putting stones on the lines of a grid, and you try to surround more space than your opponent. | ✅ |
| `how do you win tic tac toe` | You win tic tac toe by getting three of your marks in a row. The row can go across, down, or corner to corner. | ✅ |
| `what is a maze` | A maze is a puzzle of paths with walls between them. You start at one end and try to find the way through to the other. | ✅ |
| `how many checkers are in backgammon` | Each player has fifteen checkers. | ✅ |
| `what is a sliding puzzle` | A sliding puzzle has tiles in a frame with one empty space. You slide tiles into the gap, one at a time, to get them into the right order. | ✅ |
| `how do you play ludo` | Roll the dice and race all four of your tokens around the board and home. The first player home wins. | ✅ |
| `what is battleship` | Battleship is a guessing game. Each player hides ships on a grid, and you take turns guessing squares to try to find them. | ✅ |
| `how many sides does a dice have` | A dice has six sides, with one to six dots. | ✅ |

## Music

| ask | v7 answers | |
|---|---|---|
| `how many keys does a piano have` | A full size piano has eighty-eight keys. Fifty-two of them are white and thirty-six are black. | ✅ |
| `what are the note names` | There are seven note names. They are A, B, C, D, E, F and G, and then they start again at A. | ✅ |
| `how many strings does a guitar have` | A guitar has six strings. You pluck or strum them, and pressing the strings down changes the note. | ✅ |
| `what is rhythm` | Rhythm is the pattern of long and short sounds in music. The beat stays steady underneath while the rhythm dances about on top. | ✅ |

## Arithmetic - two digit

| ask | v7 answers | |
|---|---|---|
| `what is forty two plus sixteen` | Forty-two plus sixteen is fifty-eight. | ✅ |
| `what is twenty three plus fourteen` | Three plus four is seven. Twenty plus ten is thirty. Thirty plus seven is thirty-seven. So twenty-three plus fourteen is thirty-seven. | ✅ |
| `what is thirty five minus twelve` | Thirty-five minus twelve is twenty-three. | ✅ |
| `what is sixty plus twenty five` | Sixty plus twenty-five is eighty-five. | ✅ |

## Times tables

| ask | v7 answers | |
|---|---|---|
| `what is seven times eight` | Seven times eight means seven groups of eight. Five eights are forty. Two more eights are sixteen. Forty plus sixteen is fifty-six. So seven times eight... | ✅ |
| `what is six times six` | Six times six means six groups of six. Skip count by sixes: six, twelve, eighteen, twenty-four, thirty, thirty-six. So six times six is thirty-six. | ✅ |
| `what is nine times three` | Nine times three is the same as three times nine, which is easier. Three groups of nine: nine, eighteen, twenty-seven. So nine times three is twenty-seven. | ✅ |
| `what is four times five` | Four times five means four groups of five. Skip count by fives: five, ten, fifteen, twenty. So four times five is twenty. | ✅ |

## Roman numerals

| ask | v7 answers | |
|---|---|---|
| `what is ten in roman numerals` | Ten in Roman numerals is X. | ✅ |
| `what is five in roman numerals` | Five has its own letter, V. So five is V. | ✅ |
| `what is fifty in roman numerals` | Fifty has its own letter, L. So fifty is L. | ✅ |
| `what is one hundred in roman numerals` | One hundred has its own letter, C, like the word century. So one hundred is C. | ✅ |

## Elements

| ask | v7 answers | |
|---|---|---|
| `what is the symbol for gold` | The symbol for gold is Au. | ✅ |
| `what is the symbol for oxygen` | The symbol for oxygen is O. | ✅ |
| `what is the symbol for iron` | The symbol for iron is Fe. | ✅ |
| `how many protons does carbon have` | Carbon has six protons, so its atomic number is six. | ✅ |
| `how many elements are there` | There are one hundred and eighteen elements on the periodic table. | ✅ |

## Space and science

| ask | v7 answers | |
|---|---|---|
| `how many planets are there` | There are eight planets in our solar system. | ✅ |
| `what is the biggest planet` | Jupiter is the biggest planet in our solar system. It is so wide that all the other planets could fit inside it. | ✅ |
| `why do we have seasons` | Earth is tilted. As it travels around the sun, one half leans toward the sun and gets summer while the other half leans away and gets winter. | ✅ |
| `what do plants need to grow` | Plants need four things to grow. They need water, light, air and soil to hold their roots. | ✅ |
| `what is the closest planet to the sun` | Mercury is the planet closest to the sun. | ✅ |
| `what is the sun` | The sun is a star. It looks much bigger and brighter than other stars only because it is so much closer to us. | ✅ |

## Spelling and letters

| ask | v7 answers | |
|---|---|---|
| `how do you spell elephant` | Elephant is spelled e l e p h a n t. | ✅ |
| `how do you spell happy` | Happy has five letters. Happy is spelled h a p p y. | ✅ |
| `how many letters are in the alphabet` | The alphabet has twenty-six letters. It starts at a and ends at z. | ✅ |
| `what is a vowel` | The five vowels are a, e, i, o, and u. Almost every word needs at least one vowel in it. | ✅ |

## Words and meanings

| ask | v7 answers | |
|---|---|---|
| `what is a noun` | A noun is a naming word. It names a person, a place, an animal, or a thing. Dog, school, and pizza are all nouns. | ✅ |
| `what is a verb` | A verb is a doing word. It tells you what someone is doing. Run, jump, eat, and sleep are all verbs. | ✅ |
| `what is the opposite of hot` | The opposite of hot is cold. | ✅ |
| `what does huge mean` | Huge means very, very big. An elephant is huge next to a mouse. | ✅ |
| `what is the opposite of big` | The opposite of big is small. | ✅ |

## Shapes and colors

| ask | v7 answers | |
|---|---|---|
| `how many sides does a triangle have` | A triangle has three sides. | ✅ |
| `how many sides does a square have` | A square has four sides. | ✅ |
| `what colour do you get mixing red and yellow` | Red and yellow make orange. | ✅ |
| `what are the primary colours` | The primary colors of paint are red, yellow and blue. You cannot mix any other colors to make them, but they can make all the others. | ✅ |

## Jokes

| ask | v7 answers | |
|---|---|---|
| `tell me a joke` | What do you call a sleeping pizza? A pizzzza! | ✅ |
| `tell me a joke about cows` | Why did the cow go to outer space? To see the moooon! | ✅ |
| `tell me a joke about bananas` | Why did the banana go to the doctor? Because it was not peeling well! | ✅ |

## Out of scope - MUST refuse

| ask | v7 answers | |
|---|---|---|
| `what is the weather today` | I am not clever enough to answer that yet. I hope my makers teach me about that one day! | ✅ |
| `how old am i` | I am not clever enough to answer that yet. I hope my makers teach me about that one day! | ✅ |
| `what is my name` | I am not clever enough to answer that yet. I hope my makers teach me about that one day! | ✅ |
| `call my mum` | I am not clever enough to answer that yet. I hope my makers teach me about that one day! | ✅ |
| `what is on television` | I am not clever enough to answer that yet. I hope my makers teach me about that one day! | ✅ |
| `what time is it right now` | I am not clever enough to answer that yet. I hope my makers teach me about that one day! | ✅ |
| `how do you say hello in french` | I am not clever enough to answer that yet. I hope my makers teach me about that one day! | ✅ |
| `who is the president` | I am not clever enough to answer that yet. I hope my makers teach me about that one day! | ✅ |

## Fractions - FIXED in v7

| ask | v7 answers | |
|---|---|---|
| `what is half of twelve` | Half means splitting into two equal groups. Twelve splits into six and six. So half of twelve is six. | ✅ |
| `what is half of twenty` | Half means splitting into two equal groups. Twenty splits into ten and ten. So half of twenty is ten. | ✅ |
| `what is half of fifty` | Half means splitting into two equal groups. Fifty splits into twenty-five and twenty-five. So half of fifty is twenty-five. | ✅ |
| `what is a quarter of sixteen` | A quarter means splitting into four equal groups. Sixteen splits into four and four and four and four. So a quarter of sixteen is four. | ✅ |
| `what is a third of nine` | A third means splitting into three equal groups. Nine splits into three and three and three. So a third of nine is three. | ✅ |
| `what is a quarter of twenty` | A quarter means splitting into four equal groups. Twenty splits into five and five and five and five. So a quarter of twenty is five. | ✅ |
| `what is a fifth of twenty` | A fifth means splitting into five equal groups. Twenty splits into five groups of four. So a fifth of twenty is four. | ✅ |

## Percentages - FIXED in v7

| ask | v7 answers | |
|---|---|---|
| `what is fifty percent of twenty` | Fifty percent means half. Half of twenty is ten, because twenty splits into ten and ten. So fifty percent of twenty is ten. | ✅ |
| `what is fifty percent of forty` | Fifty percent means half. Half of forty is twenty, because forty splits into twenty and twenty. So fifty percent of forty is twenty. | ✅ |
| `what is twenty five percent of forty` | Twenty-five percent of forty is ten. | ✅ |
| `what is ten percent of fifty` | Ten percent means ten out of every hundred, which is one tenth. A tenth of fifty is five, because fifty splits into ten groups of five. So it is five. | ✅ |
| `what is one hundred percent of thirty` | One hundred percent means all of it. So one hundred percent of thirty is thirty. | ✅ |

## KNOWN WEAK - expect failures

| ask | v7 answers | |
|---|---|---|
| `what is half of twelve` | Half means splitting into two equal groups. Twelve splits into six and six. So half of twelve is six. | ✅ |
| `what is fifty percent of twenty` | Fifty percent means half. Half of twenty is ten, because twenty splits into ten and ten. So fifty percent of twenty is ten. | ✅ |
| `how many cents in a quarter` | A quarter is worth twenty-five cents. It is called a quarter because twenty-five cents is a quarter of a dollar. | ✅ |
| `what time is half past three` | Half past means thirty minutes, so the long hand points straight down at six. The short hand sits halfway between three and four. That is half past three. | ✅ |
| `what is a baby cow called` | A calf is a baby cow. | ✅ |
| `what is the fastest animal` | The cheetah is the fastest animal on land. It can run faster than a car drives in town, but only for a short burst. | ✅ |

---

## What changed in v7

| | v6 | v7 |
|---|---|---|
| fractions & percentages | 17.5% | **91.2%** |
| held-out exact | 53.0% | **60.9%** |
| over-refusal | 4.0% | **3.3%** |

Fractions and percentages were confidently wrong in v5 and v6 — `half of
twelve` returned *four*. Fixed by enumerating all 251 facts in the space with
every answer asserted against its computed value.

## Still weak in v7

Word problems, multi-coin money questions, and anything needing today or now.

Note that some scored failures are actually correct: *"Hartford is the capital
of Connecticut"* is marked wrong only because training says *"The capital of
Connecticut is Hartford"*. Geography's true accuracy is above its reported
75.0%.

Per-field `arithmetic 2-digit` reads 35.0%, but the grader that checks whether
the final number is right gives **82.3%** for the same model. The per-field
metric demands exact string match across a whole worked-steps derivation.
Trust 82.3%.

