"""Music, counting on hands, dice, chance, reaction, odd one out, cinnamon.

Like games_board, every topic here was a blank in the old corpus.

The chance topics are kept deliberately shallow. "It is just as likely to be
heads as tails" is the right level for a five year old; anything about
independence of events or expected value is not, and a half-explained
probability rule is the kind of thing a child repeats wrongly for years.
"""
from . import Fact, report


def F(topic, answer, train, evalq):
    return Fact(topic=topic, train=list(train), evalq=list(evalq), answer=answer)


MUS = "music"
HAND = "counting on hands"
DICE = "dice & chance"
CHANCE = "chance"
REACT = "reaction game"
ODD = "odd one out"
CINN = "cinnamon game"


_MUSIC = [
    ("Note names",
     "There are seven note names. They are A, B, C, D, E, F and G, and then "
     "they start again at A.",
     ["what are the note names", "how many notes are there in music",
      "name the musical notes", "what letters are the notes"],
     ["can you list the note names"]),
    ("Piano keys",
     "A full size piano has eighty-eight keys. Fifty-two of them are white and "
     "thirty-six are black.",
     ["how many keys does a piano have", "how many keys are on a piano",
      "number of piano keys", "how big is a piano keyboard"],
     ["how many keys are there on a full piano"]),
    ("Black and white keys",
     "The white keys play the plain notes A to G. The black keys play the notes "
     "in between, and they come in groups of two and three.",
     ["what are the black keys for", "why does a piano have black keys",
      "what is the difference between black and white keys",
      "how are the black keys grouped"],
     ["what do the black piano keys do"]),
    ("Sharp",
     "A sharp makes a note a little bit higher. On a piano it is usually the "
     "black key just to the right.",
     ["what is a sharp in music", "what does sharp mean",
      "what does a sharp do to a note", "tell me about sharps"],
     ["can you explain what a sharp is"]),
    ("Flat",
     "A flat makes a note a little bit lower. On a piano it is usually the "
     "black key just to the left.",
     ["what is a flat in music", "what does flat mean",
      "what does a flat do to a note", "tell me about flats"],
     ["can you explain what a flat is"]),
    ("What sound is",
     "Sound happens when something shakes very fast. The shaking wobbles the "
     "air, the air wobbles your eardrum, and you hear it.",
     ["what is sound", "how does sound work", "what makes a sound",
      "how do i hear things"],
     ["can you explain how sound is made"]),
    ("String instruments",
     "String instruments make sound from shaking strings. The guitar, violin, "
     "cello and harp are all string instruments.",
     ["what are string instruments", "name some string instruments",
      "which instruments have strings", "tell me about string instruments"],
     ["which instruments use strings"]),
    ("Wind instruments",
     "Wind instruments make sound when you blow air through them. The flute, "
     "recorder and clarinet are wind instruments.",
     ["what are wind instruments", "name some wind instruments",
      "which instruments do you blow", "tell me about wind instruments"],
     ["which instruments do you blow into"]),
    ("Brass instruments",
     "Brass instruments are metal and you buzz your lips to play them. The "
     "trumpet, trombone and tuba are brass instruments.",
     ["what are brass instruments", "name some brass instruments",
      "which instruments are made of brass", "tell me about brass instruments"],
     ["which instruments are brass ones"]),
    ("Percussion",
     "Percussion instruments make sound when you hit or shake them. Drums, "
     "tambourines, triangles and cymbals are percussion.",
     ["what are percussion instruments", "name some percussion instruments",
      "which instruments do you hit", "tell me about percussion"],
     ["which instruments do you bang or shake"]),
    ("Beat",
     "The beat is the steady pulse in music, the part you tap your foot to. It "
     "keeps going at the same speed all the way through.",
     ["what is a beat in music", "what does beat mean",
      "what do you tap your foot to", "tell me about the beat"],
     ["can you explain what a beat is"]),
    ("Rhythm",
     "Rhythm is the pattern of long and short sounds in music. The beat stays "
     "steady underneath while the rhythm dances about on top.",
     ["what is rhythm", "what does rhythm mean",
      "what is the difference between beat and rhythm", "tell me about rhythm"],
     ["can you explain rhythm to me"]),
    ("Loud and soft",
     "Music can be loud or soft, and changing between them makes it "
     "interesting. Musicians use the words forte for loud and piano for soft.",
     ["what does loud and soft mean in music", "how do you make music loud",
      "what is forte", "what does piano mean in music"],
     ["what are the music words for loud and soft"]),
    ("Fast and slow",
     "How fast music goes is called the tempo. A fast tempo feels exciting and "
     "a slow one feels calm.",
     ["what is tempo", "what does tempo mean", "how fast should music go",
      "what makes music fast or slow"],
     ["can you explain tempo"]),
    ("High and low",
     "A high note sounds like a bird or a whistle, and a low note sounds like a "
     "big drum or a growl. How high or low a note is, is called its pitch.",
     ["what is pitch", "what makes a note high or low",
      "what does high and low mean in music", "what is a high note"],
     ["can you explain pitch in music"]),
    ("Melody",
     "The melody is the tune, the part you sing along to and remember after the "
     "music stops.",
     ["what is a melody", "what does melody mean", "what is a tune",
      "which part of a song do you sing"],
     ["can you explain what a melody is"]),
    ("Singing",
     "When you sing, air from your lungs shakes two small folds in your throat. "
     "Everyone's voice sounds a bit different.",
     ["how do i sing", "what happens when i sing", "how does singing work",
      "why does my voice sound like me"],
     ["can you explain how singing works"]),
    ("Orchestra",
     "An orchestra is a big group of musicians playing together. A conductor "
     "stands at the front and keeps everyone in time.",
     ["what is an orchestra", "what does a conductor do",
      "who leads an orchestra", "tell me about orchestras"],
     ["can you explain what an orchestra is"]),
    ("Drum",
     "A drum makes sound when you hit its tight skin and it shakes. A bigger "
     "drum makes a deeper sound.",
     ["how does a drum work", "what is a drum", "why do big drums sound deep",
      "tell me about drums"],
     ["can you explain how a drum makes sound"]),
    ("Guitar",
     "A guitar has six strings. You pluck or strum them, and pressing the "
     "strings down changes the note.",
     ["how many strings does a guitar have", "how does a guitar work",
      "tell me about guitars", "how do you play a guitar"],
     ["how many strings are on a guitar"]),
    ("Why practice",
     "Practicing a little every day works better than a lot all at once. Your "
     "fingers and your brain both need time to learn the pattern.",
     ["how do i get better at music", "why should i practice",
      "how often should i practice", "what is the best way to learn an instrument"],
     ["what helps you improve at an instrument"]),
]

_HANDS = [
    ("Fingers on a hand",
     "You have five fingers on each hand, and ten fingers altogether.",
     ["how many fingers are on one hand", "how many fingers do you have on a hand",
      "how many fingers altogether", "how many fingers on two hands"],
     ["what is the number of fingers on a hand"]),
    ("Counting in fives",
     "Hands are why people count in fives and tens. One hand is five, and two "
     "hands make ten.",
     ["why do we count in fives", "why do we count in tens",
      "what do hands have to do with counting", "why is ten a special number"],
     ["why do people count by fives and tens"]),
    ("Showing a number",
     "To show a number on your fingers, put up that many. For seven, put up all "
     "five on one hand and two on the other. Five and two is seven.",
     ["how do i show seven on my fingers", "how do you count on your hands",
      "how do i show a number with my fingers", "how do i make seven with my hands"],
     ["how would you show seven on your fingers"]),
    ("Tally marks",
     "Tally marks are little lines for counting. You draw four lines, then a "
     "fifth one across them, so each bundle is five.",
     ["what are tally marks", "how do tally marks work",
      "how do you count with lines", "why is the fifth tally across"],
     ["can you explain tally marks"]),
    ("Counting on fingers is fine",
     "Counting on your fingers is a good thing to do. It helps you see what is "
     "happening, and everyone starts that way.",
     ["is it ok to count on my fingers", "should i count on my fingers",
      "is finger counting bad", "do grown ups count on fingers"],
     ["is counting on fingers allowed"]),
]

_DICE = [
    ("Dice sides",
     "A normal die has six sides, with one to six dots on them.",
     ["how many sides does a die have", "how many sides on a dice",
      "what numbers are on a dice", "how many faces does a die have"],
     ["how many sides are there on a die"]),
    ("Opposite faces",
     "On a die, opposite faces always add up to seven. One and six is seven. "
     "Two and five is seven. Three and four is seven.",
     ["what do opposite sides of a dice add up to",
      "what is on the other side of a dice", "what is opposite the one on a die",
      "why do dice add to seven"],
     ["what do the opposite faces of a die total"]),
    ("Highest roll",
     "The highest you can roll on one die is six, because six is the biggest "
     "number on it.",
     ["what is the highest number on a dice", "what is the biggest roll",
      "what is the most you can roll", "what is the top number on a die"],
     ["what is the largest number you can roll"]),
    ("Two dice highest",
     "With two dice the most you can roll is twelve. Six plus six is twelve.",
     ["what is the highest you can roll with two dice",
      "what is the biggest total with two dice",
      "what is the most two dice can make", "what is double six worth"],
     ["what is the largest total on two dice"]),
    ("Two dice lowest",
     "With two dice the lowest you can roll is two. One plus one is two.",
     ["what is the lowest you can roll with two dice",
      "what is the smallest total with two dice",
      "what is the least two dice can make", "what is double one worth"],
     ["what is the smallest total on two dice"]),
    ("Dice are fair",
     "A fair die has the same chance of landing on every side. That is what "
     "makes a dice game fair for everyone.",
     ["is a dice fair", "what does a fair dice mean",
      "does every number come up the same", "why do we use dice in games"],
     ["what makes a die fair"]),
    ("What luck is",
     "Luck is when something happens that you did not choose and cannot "
     "control, like which number a die lands on.",
     ["what is luck", "what does luck mean", "what is chance",
      "can you control luck"],
     ["can you explain what luck is"]),
    ("Likely and unlikely",
     "Likely means something will probably happen, and unlikely means it "
     "probably will not. Rain in winter is likely, and snow in summer is "
     "unlikely.",
     ["what does likely mean", "what does unlikely mean",
      "what is the difference between likely and unlikely",
      "give me an example of unlikely"],
     ["can you explain likely and unlikely"]),
    ("Taking turns",
     "Taking turns means everyone gets a go, one after another. It is the "
     "fairest way to play a game together.",
     ["why do we take turns", "what does taking turns mean",
      "why is taking turns fair", "how do turns work in a game"],
     ["why is it good to take turns"]),
]

_CHANCE = [
    ("Heads or tails",
     "A coin has two sides, heads and tails. When you flip it, it is just as "
     "likely to land on one as the other.",
     ["what is heads or tails", "how many sides does a coin have",
      "what happens when you flip a coin", "is a coin flip fair"],
     ["can you explain flipping a coin"]),
    ("Fifty fifty",
     "Fifty fifty means two things are equally likely. A coin flip is fifty "
     "fifty, because heads and tails have the same chance.",
     ["what does fifty fifty mean", "what is a fifty fifty chance",
      "what does equal chance mean", "when is something fifty fifty"],
     ["can you explain fifty fifty"]),
    ("Past flips",
     "Each coin flip starts fresh. Even if you got heads three times, the next "
     "flip is still just as likely to be heads as tails.",
     ["does the last coin flip change the next one",
      "if i get heads three times what comes next",
      "does a coin remember what it did", "is the next flip more likely to be tails"],
     ["does what happened before change the next flip"]),
    ("Fair ways to choose",
     "Flipping a coin, rolling a die or drawing straws are all fair ways to "
     "choose, because nobody can decide the answer in advance.",
     ["what is a fair way to choose", "how do we decide who goes first",
      "how can we choose fairly", "what is a fair way to pick"],
     ["what are some fair ways to decide"]),
    ("Guessing",
     "A guess is an answer you are not sure about. Guessing is fine when you "
     "cannot know, but it is better to work it out if you can.",
     ["what is a guess", "is it ok to guess", "what does guessing mean",
      "when should i guess"],
     ["can you explain what guessing is"]),
]

_REACT = [
    ("Reaction time",
     "Reaction time is how long it takes you to move after you see something. "
     "It is very short, but it is never zero.",
     ["what is reaction time", "what does reaction time mean",
      "how fast can i react", "how quick is a reaction"],
     ["can you explain reaction time"]),
    ("Getting faster",
     "Practicing makes your reactions faster. Being rested helps too, because "
     "a tired brain is slower.",
     ["how do i get faster reactions", "can i improve my reaction time",
      "how do i get quicker", "what makes reactions faster"],
     ["what helps your reactions get quicker"]),
    ("Hand eye coordination",
     "Hand eye coordination is your hands and eyes working together. Catching a "
     "ball needs it, and so does tapping the right spot quickly.",
     ["what is hand eye coordination", "what does hand eye coordination mean",
      "how do my hands and eyes work together", "what helps me catch a ball"],
     ["can you explain hand eye coordination"]),
    ("Moles",
     "A mole is a small furry animal that digs tunnels under the ground. Moles "
     "have big strong front paws for digging and very tiny eyes.",
     ["what is a mole", "tell me about moles", "where do moles live",
      "why do moles have big paws"],
     ["can you tell me about the mole animal"]),
    ("Fast and slow",
     "Fast means moving in a short time and slow means taking longer. A cheetah "
     "is fast and a snail is slow.",
     ["what does fast mean", "what does slow mean",
      "what is the difference between fast and slow", "what animal is slow"],
     ["can you explain fast and slow"]),
]

_ODD = [
    ("Odd one out",
     "The odd one out is the thing in a group that does not belong with the "
     "others. To find it, look for what most of them have in common.",
     ["what is the odd one out", "how do you find the odd one out",
      "what does odd one out mean", "how do i play odd one out"],
     ["can you explain the odd one out game"]),
    ("Sorting",
     "Sorting means putting things into groups that go together, like all the "
     "red ones in one pile and all the blue ones in another.",
     ["what is sorting", "what does sorting mean", "how do you sort things",
      "what does it mean to sort"],
     ["can you explain sorting"]),
    ("A group",
     "A group is a set of things that belong together for some reason. Animals, "
     "colors and shapes are all groups.",
     ["what is a group", "what is a category", "what does group mean",
      "how do things belong together"],
     ["can you explain what a group is"]),
    ("Same and different",
     "Same means alike in some way, and different means not alike. Two red "
     "balls are the same color but might be different sizes.",
     ["what does same mean", "what does different mean",
      "what is the difference between same and different",
      "how can things be the same and different"],
     ["can you explain same and different"]),
    ("What they have in common",
     "Having something in common means sharing it. A cat and a dog have fur in "
     "common, and both have four legs.",
     ["what does in common mean", "what do a cat and a dog have in common",
      "how do i find what is the same", "what does sharing a feature mean"],
     ["can you explain having something in common"]),
]

_CINN = [
    ("What cinnamon is",
     "Cinnamon is a spice that comes from the bark of a tree. It is rolled into "
     "little sticks or ground into a brown powder.",
     ["what is cinnamon", "where does cinnamon come from",
      "what is cinnamon made of", "tell me about cinnamon"],
     ["can you tell me what cinnamon is"]),
    ("What a spice is",
     "A spice is a part of a plant used to give food flavor. Cinnamon, pepper "
     "and ginger are all spices.",
     ["what is a spice", "what are spices", "name some spices",
      "what do spices do"],
     ["can you explain what a spice is"]),
    ("Cinnamon taste",
     "Cinnamon tastes warm and a little bit sweet. People put it on toast, in "
     "cakes and in hot drinks.",
     ["what does cinnamon taste like", "what is cinnamon used for",
      "what food has cinnamon in it", "is cinnamon sweet"],
     ["how would you describe the taste of cinnamon"]),
    ("Taste and smell together",
     "Tasting and smelling work together. If you hold your nose while you eat, "
     "food tastes much more boring.",
     ["why does food taste boring when my nose is blocked",
      "do taste and smell work together", "why do i need my nose to taste",
      "what happens if i hold my nose and eat"],
     ["how are tasting and smelling connected"]),
]


def build():
    facts = []
    for topic, table in ((MUS, _MUSIC), (HAND, _HANDS), (DICE, _DICE),
                         (CHANCE, _CHANCE), (REACT, _REACT), (ODD, _ODD),
                         (CINN, _CINN)):
        for _name, answer, train, evalq in table:
            facts.append(F(topic, answer, train, evalq))
    return facts


if __name__ == "__main__":
    from collections import Counter
    f = build()
    report("games_play", f)
    for t, n in Counter(x.topic for x in f).most_common():
        print(f"   {t:20s} {n}")
