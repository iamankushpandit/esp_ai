"""Chemistry, colors, and shapes.

Chemistry here is DELIBERATELY TINY. The previous corpus taught all one
hundred and eighteen elements - symbol, proton count, category - and that cost
5,690 training samples, 2.9% of the corpus, on facts like the proton count of
samarium. No five-year-old asks that, and the capacity went missing from
topics they do ask about. What survives is the handful of substances a child
actually meets, plus what an atom is and the three states of matter.

Colors and shapes are the opposite case: they were barely present before and
are asked constantly.
"""
from . import Fact, report


def F(topic, answer, train, evalq):
    return Fact(topic=topic, train=list(train), evalq=list(evalq), answer=answer)


CHEM = "chemistry"
COL = "colors"
SHP = "shapes"


# ------------------------------------------------------------- chemistry
# (name, answer, [train phrasings], [held-out phrasing])
_CHEM = [
    ("What is an atom",
     "An atom is the tiniest piece of something. Everything around you is made "
     "of atoms, and they are far too small to see.",
     ["what is an atom", "what are atoms", "tell me about atoms",
      "what is everything made of"],
     ["can you explain what an atom is"]),
    ("Air",
     "Air is a mix of gases. Most of it is nitrogen, and about one part in five "
     "is oxygen, which is the part we need to breathe.",
     ["what is air made of", "what is in the air", "what is air",
      "tell me about air"],
     ["what stuff makes up air"]),
    ("Oxygen",
     "Oxygen is the gas we need to breathe. Plants and trees make it, which is "
     "one reason they matter so much.",
     ["what is oxygen", "tell me about oxygen", "what does oxygen do",
      "why do we need oxygen"],
     ["what is oxygen for"]),
    ("Water",
     "Water is made of two tiny parts. It has hydrogen and oxygen joined "
     "together, which is why people call it H two O.",
     ["what is water made of", "what is water", "tell me about water",
      "what is in water"],
     ["what is water made out of"]),
    ("Hydrogen",
     "Hydrogen is the lightest thing there is. It is part of water, and stars "
     "like our Sun are mostly made of it.",
     ["what is hydrogen", "tell me about hydrogen", "what is the lightest gas",
      "where is hydrogen found"],
     ["can you tell me about hydrogen"]),
    ("Helium",
     "Helium is a very light gas. It is what makes party balloons float up "
     "instead of falling down.",
     ["what is helium", "what makes balloons float", "tell me about helium",
      "why do balloons go up"],
     ["what gas is in a floating balloon"]),
    ("Carbon",
     "Carbon is in every living thing. Pencil lead is carbon, and so is the "
     "black part of burnt toast.",
     ["what is carbon", "tell me about carbon", "where is carbon found",
      "what is pencil lead made of"],
     ["what is carbon used in"]),
    ("Gold",
     "Gold is a shiny yellow metal. People make rings and coins from it, and it "
     "does not rust, so it stays shiny for a very long time.",
     ["what is gold", "tell me about gold", "why is gold special",
      "what color is gold"],
     ["what makes gold different from other metals"]),
    ("Iron",
     "Iron is a strong gray metal. It is used to make tools and buildings, and "
     "it goes rusty and orange if it stays wet.",
     ["what is iron", "tell me about iron", "what is iron used for",
      "what metal rusts"],
     ["why does iron turn orange"]),
    ("Metal",
     "Metals are hard and shiny and can be bent without breaking. Gold, iron, "
     "silver and copper are all metals.",
     ["what is a metal", "what are metals", "name some metals",
      "tell me about metals"],
     ["which things are metals"]),
    ("States of matter",
     "There are three main kinds. Solid keeps its shape, liquid pours and takes "
     "the shape of its cup, and gas spreads out to fill the whole room.",
     ["what are the three states of matter", "what is solid liquid and gas",
      "what are the states of matter", "tell me about solids liquids and gases"],
     ["what are the three kinds of matter"]),
    ("Solid",
     "A solid keeps its own shape. A rock, a chair and an ice cube are all "
     "solids.",
     ["what is a solid", "tell me about solids", "give me an example of a solid",
      "what does solid mean"],
     ["what makes something a solid"]),
    ("Liquid",
     "A liquid pours and takes the shape of whatever holds it. Water, milk and "
     "juice are liquids.",
     ["what is a liquid", "tell me about liquids", "what does liquid mean",
      "give me an example of a liquid"],
     ["what makes something a liquid"]),
    ("Gas",
     "A gas spreads out to fill all the space it can. Air is a gas, and so is "
     "the helium in a balloon.",
     ["what is a gas", "tell me about gases", "what does gas mean",
      "give me an example of a gas"],
     ["what makes something a gas"]),
    ("Ice",
     "Ice is water that got cold enough to turn solid. Warm it up and it melts "
     "back into water again.",
     ["what is ice", "how is ice made", "what happens when water freezes",
      "why does water turn to ice"],
     ["what makes water become ice"]),
    ("Steam",
     "Steam is water that got hot enough to turn into a gas. That is the cloud "
     "you see above a boiling pot.",
     ["what is steam", "what happens when water boils", "how is steam made",
      "what is the cloud over a kettle"],
     ["where does steam come from"]),
    ("Melting",
     "Melting is when a solid gets warm and turns into a liquid. Ice melting "
     "into water is the easiest one to watch.",
     ["what is melting", "what does melting mean", "what happens when ice melts",
      "tell me about melting"],
     ["can you explain melting"]),
    ("Freezing",
     "Freezing is when a liquid gets cold and turns into a solid. Water freezes "
     "into ice in the freezer.",
     ["what is freezing", "what does freezing mean", "tell me about freezing",
      "how do you freeze water"],
     ["can you explain freezing"]),
    ("Mixing",
     "Some things mix into water and seem to vanish, like sugar and salt. Others "
     "do not mix at all, like oil, which floats on top.",
     ["what dissolves in water", "does sugar mix with water",
      "why does oil float on water", "what happens when you mix sugar and water"],
     ["which things mix into water"]),
]

# ---------------------------------------------------------------- colors
_COLORS = [
    ("Primary colors",
     "The primary colors of paint are red, yellow and blue. You cannot mix any "
     "other colors to make them, but they can make all the others.",
     ["what are the primary colors", "what are the main colors",
      "name the primary colors", "which colors are primary"],
     ["what colors are the primary ones"]),
    ("Secondary colors",
     "The secondary colors are orange, green and purple. Each one comes from "
     "mixing two primary colors together.",
     ["what are the secondary colors", "name the secondary colors",
      "which colors are secondary", "what colors do you get from mixing"],
     ["what are the mixed colors called"]),
    ("Red and yellow",
     "Red and yellow make orange.",
     ["what do red and yellow make", "what color is red mixed with yellow",
      "if i mix red and yellow what do i get", "red plus yellow is what color"],
     ["what happens when you mix red and yellow"]),
    ("Blue and yellow",
     "Blue and yellow make green.",
     ["what do blue and yellow make", "what color is blue mixed with yellow",
      "if i mix blue and yellow what do i get", "blue plus yellow is what color"],
     ["what happens when you mix blue and yellow"]),
    ("Red and blue",
     "Red and blue make purple.",
     ["what do red and blue make", "what color is red mixed with blue",
      "if i mix red and blue what do i get", "red plus blue is what color"],
     ["what happens when you mix red and blue"]),
    ("Black and white",
     "Black and white make gray.",
     ["what do black and white make", "what color is black mixed with white",
      "if i mix black and white what do i get", "how do you make gray"],
     ["what happens when you mix black and white"]),
    ("Making pink",
     "Red and white make pink. The more white you add, the paler the pink gets.",
     ["how do you make pink", "what makes pink", "what colors make pink",
      "how do i mix pink"],
     ["which colors do you mix for pink"]),
    ("Making brown",
     "Mixing all three primary colors makes brown. Red, yellow and blue "
     "together give you a muddy brown.",
     ["how do you make brown", "what makes brown", "what colors make brown",
      "how do i mix brown"],
     ["which colors do you mix for brown"]),
    ("Rainbow colors",
     "A rainbow has red, orange, yellow, green, blue, indigo and violet, always "
     "in that order.",
     ["what are the colors of the rainbow", "name the rainbow colors",
      "how many colors in a rainbow", "what order are rainbow colors in"],
     ["which colors make up a rainbow"]),
    ("How rainbows form",
     "Rainbows happen when sunlight shines through raindrops. Each drop bends "
     "the light and splits it into all its colors.",
     ["how does a rainbow form", "why do rainbows happen", "what makes a rainbow",
      "where do rainbows come from"],
     ["can you explain how a rainbow is made"]),
    ("White light",
     "White light is all the colors mixed together. A raindrop or a glass prism "
     "can split it apart so you see them.",
     ["what is white light", "is white a color", "what is sunlight made of",
      "why is light white"],
     ["what colors are inside white light"]),
    ("Why leaves change color",
     "Leaves are green because of a coloring inside them. In autumn the tree "
     "stops making it, and the yellow and orange that were always there show "
     "through.",
     ["why do leaves change color", "why do leaves turn orange",
      "what happens to leaves in autumn", "why are leaves not green in autumn"],
     ["what makes leaves change color"]),
    ("Why the sky is blue",
     "Sunlight has every color in it. Blue light bounces around the air the "
     "most, so it reaches your eyes from all over the sky.",
     ["why is the sky blue", "what makes the sky blue", "why is the sky that color",
      "how come the sky is blue"],
     ["can you explain why the sky looks blue"]),
    ("Why grass is green",
     "Grass and leaves are green because of a coloring inside them that helps "
     "them catch sunlight and make food.",
     ["why is grass green", "what makes grass green", "why are plants green",
      "why are leaves green"],
     ["what makes plants look green"]),
    ("Warm and cool colors",
     "Red, orange and yellow are called warm colors because they feel like fire "
     "and sunshine. Blue, green and purple are called cool colors.",
     ["what are warm colors", "what are cool colors",
      "which colors are warm and which are cool", "tell me about warm and cool colors"],
     ["what makes a color warm or cool"]),
    ("Shades",
     "Adding white to a color makes it lighter and adding black makes it darker. "
     "That is how you get light blue and dark blue from the same blue.",
     ["how do you make a color lighter", "how do you make a color darker",
      "what is a shade", "how do i get light blue"],
     ["how do you change how dark a color is"]),
    ("Light mixing",
     "Paint and light mix differently. Mixing all the paints gives muddy brown, "
     "but mixing all the colored lights gives white.",
     ["is mixing light the same as mixing paint",
      "what happens if you mix all the colors of light",
      "why do paint and light mix differently",
      "what do all the colored lights make together"],
     ["how is light mixing different from paint mixing"]),
]

# ---------------------------------------------------------------- shapes
# (shape name in speech, display name, side count word, corner count word)
_POLY = [
    ("triangle", "A triangle", "three", "three"),
    ("square", "A square", "four", "four"),
    ("rectangle", "A rectangle", "four", "four"),
    ("pentagon", "A pentagon", "five", "five"),
    ("hexagon", "A hexagon", "six", "six"),
    ("octagon", "An octagon", "eight", "eight"),
]

_SHAPES = [
    ("Circle",
     "A circle is perfectly round. It has no sides and no corners at all.",
     ["what is a circle", "how many sides does a circle have",
      "how many corners does a circle have", "tell me about circles"],
     ["describe a circle for me"]),
    ("Square vs rectangle",
     "A square is a special rectangle. Both have four corners, but a square has "
     "all four sides the same length.",
     ["what is the difference between a square and a rectangle",
      "is a square a rectangle", "how is a square different from a rectangle",
      "why is a square special"],
     ["can you tell me how squares and rectangles differ"]),
    ("Cube",
     "A cube is a solid shape with six flat square faces, like a dice or a "
     "sugar lump.",
     ["what is a cube", "how many faces does a cube have", "tell me about cubes",
      "what shape is a dice"],
     ["describe a cube for me"]),
    ("Sphere",
     "A sphere is a ball shape. It is round all the way around, with no flat "
     "sides at all.",
     ["what is a sphere", "what shape is a ball", "tell me about spheres",
      "what is a round solid shape called"],
     ["describe a sphere for me"]),
    ("Cylinder",
     "A cylinder is the shape of a tin can. It has a flat circle at each end "
     "and a curved side in between.",
     ["what is a cylinder", "what shape is a can", "tell me about cylinders",
      "what shape is a tube"],
     ["describe a cylinder for me"]),
    ("Cone",
     "A cone has a circle at the bottom and rises to a point at the top, like "
     "an ice cream cone or a traffic cone.",
     ["what is a cone", "what shape is an ice cream cone", "tell me about cones",
      "what shape comes to a point"],
     ["describe a cone for me"]),
    ("Pyramid",
     "A pyramid has a flat shape at the bottom and triangle sides that meet at "
     "a point on top.",
     ["what is a pyramid", "tell me about pyramids", "what shape is a pyramid",
      "what does a pyramid look like"],
     ["describe a pyramid for me"]),
    ("Two d and three d",
     "Flat shapes are called two d, like a drawing of a square. Solid shapes "
     "you can hold are three d, like a cube.",
     ["what is the difference between two d and three d",
      "what does two d mean", "what does three d mean",
      "what is a flat shape called"],
     ["how are flat and solid shapes different"]),
    ("Faces edges corners",
     "A face is a flat side you could put your hand on. An edge is where two "
     "faces meet, and a corner is where the edges come together in a point.",
     ["what is a face on a shape", "what is an edge", "what is a corner",
      "tell me about faces and edges"],
     ["what do face and edge mean on a shape"]),
    ("Symmetry",
     "A shape is symmetrical if you can fold it in half and both halves match. "
     "A butterfly and a heart are both symmetrical.",
     ["what is symmetry", "what does symmetrical mean", "tell me about symmetry",
      "what is a line of symmetry"],
     ["can you explain symmetry to me"]),
    ("Oval",
     "An oval is a stretched circle, the shape of an egg. Like a circle, it has "
     "no corners.",
     ["what is an oval", "what shape is an egg", "tell me about ovals",
      "what is a stretched circle called"],
     ["describe an oval for me"]),
    ("Shapes around us",
     "Shapes are everywhere. Wheels and plates are circles, doors and books are "
     "rectangles, and a slice of pizza is a triangle.",
     ["where do you see shapes", "what shapes are around us",
      "name some shapes in real life", "what things are shaped like circles"],
     ["can you name shapes you see every day"]),
]


def build():
    facts = []

    for _name, answer, train, evalq in _CHEM:
        facts.append(F(CHEM, answer, train, evalq))
    for _name, answer, train, evalq in _COLORS:
        facts.append(F(COL, answer, train, evalq))
    for _name, answer, train, evalq in _SHAPES:
        facts.append(F(SHP, answer, train, evalq))

    # Side and corner counts are mechanical, so generate them rather than
    # typing six near-identical blocks and risking a transcription slip.
    for spoken, display, sides, corners in _POLY:
        facts.append(F(SHP,
                       f"{display} has {sides} sides.",
                       [f"how many sides does a {spoken} have",
                        f"how many sides has a {spoken} got",
                        f"number of sides on a {spoken}",
                        f"tell me how many sides a {spoken} has"],
                       [f"how many sides are there on a {spoken}"]))
        facts.append(F(SHP,
                       f"{display} has {corners} corners.",
                       [f"how many corners does a {spoken} have",
                        f"how many corners has a {spoken} got",
                        f"number of corners on a {spoken}",
                        f"how many points does a {spoken} have"],
                       [f"how many corners are there on a {spoken}"]))

    return facts


if __name__ == "__main__":
    from collections import Counter
    f = build()
    report("science_matter", f)
    for t, n in Counter(x.topic for x in f).most_common():
        print(f"   {t:12s} {n}")
