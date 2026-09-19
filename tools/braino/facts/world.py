"""World and US geography, flags, maps, calendar patterns, community, feelings.

Deliberately SMALL on countries. The previous corpus tried to teach all one
hundred and ninety five capitals and measured 61.2% - the failures were
interference between look-alike facts ("Conakry is the capital of South
Dakota"). Forty countries a child actually meets, taught in both directions
with four phrasings each, is worth more than one hundred and ninety five
half-learned ones.
"""
from . import Fact, report


def F(topic, answer, train, evalq):
    return Fact(topic=topic, train=list(train), evalq=list(evalq), answer=answer)


WORLD = "world geography"
USGEO = "US geography"
FLAGS = "flags"
MAPS = "maps"
CAL = "calendar & patterns"
SAFE = "community & safety"
FEEL = "feelings"


# ---------------------------------------------------------------- countries
# (question form, display form, capital question form, capital display form)
COUNTRIES = [
    ("united states", "the United States", "washington dc", "Washington DC"),
    ("canada", "Canada", "ottawa", "Ottawa"),
    ("mexico", "Mexico", "mexico city", "Mexico City"),
    ("brazil", "Brazil", "brasilia", "Brasilia"),
    ("argentina", "Argentina", "buenos aires", "Buenos Aires"),
    ("peru", "Peru", "lima", "Lima"),
    ("chile", "Chile", "santiago", "Santiago"),
    ("the united kingdom", "the United Kingdom", "london", "London"),
    ("ireland", "Ireland", "dublin", "Dublin"),
    ("france", "France", "paris", "Paris"),
    ("spain", "Spain", "madrid", "Madrid"),
    ("portugal", "Portugal", "lisbon", "Lisbon"),
    ("italy", "Italy", "rome", "Rome"),
    ("germany", "Germany", "berlin", "Berlin"),
    ("the netherlands", "the Netherlands", "amsterdam", "Amsterdam"),
    ("switzerland", "Switzerland", "bern", "Bern"),
    ("norway", "Norway", "oslo", "Oslo"),
    ("sweden", "Sweden", "stockholm", "Stockholm"),
    ("denmark", "Denmark", "copenhagen", "Copenhagen"),
    ("poland", "Poland", "warsaw", "Warsaw"),
    ("austria", "Austria", "vienna", "Vienna"),
    ("greece", "Greece", "athens", "Athens"),
    ("iceland", "Iceland", "reykjavik", "Reykjavik"),
    ("russia", "Russia", "moscow", "Moscow"),
    ("egypt", "Egypt", "cairo", "Cairo"),
    ("kenya", "Kenya", "nairobi", "Nairobi"),
    ("nigeria", "Nigeria", "abuja", "Abuja"),
    ("morocco", "Morocco", "rabat", "Rabat"),
    ("south africa", "South Africa", "pretoria", "Pretoria"),
    ("china", "China", "beijing", "Beijing"),
    ("japan", "Japan", "tokyo", "Tokyo"),
    ("india", "India", "new delhi", "New Delhi"),
    ("south korea", "South Korea", "seoul", "Seoul"),
    ("thailand", "Thailand", "bangkok", "Bangkok"),
    ("vietnam", "Vietnam", "hanoi", "Hanoi"),
    ("nepal", "Nepal", "kathmandu", "Kathmandu"),
    ("turkey", "Turkey", "ankara", "Ankara"),
    ("saudi arabia", "Saudi Arabia", "riyadh", "Riyadh"),
    ("australia", "Australia", "canberra", "Canberra"),
    ("new zealand", "New Zealand", "wellington", "Wellington"),
]

# Countries whose capital needs a note instead of the plain template.
CAPITAL_NOTE = {
    "south africa": "The capital of South Africa is Pretoria. South Africa is "
                    "unusual because it has three capital cities.",
}
CAPITAL_NOTE_BACK = {
    "pretoria": "Pretoria is a capital city of South Africa. South Africa has "
                "three capital cities in all.",
}


def _capitals(topic, rows, notes, notes_back, kind):
    """kind is 'country' or 'state'. Builds both directions."""
    out = []
    for q, disp, capq, capdisp in rows:
        if kind == "country":
            fwd_train = [
                f"what is the capital of {q}",
                f"what is the capital city of {q}",
                f"{q} capital",
                f"whats the capital of {q}",
            ]
            fwd_eval = [f"tell me the capital of {q}"]
            back_train = [
                f"{capq} is the capital of which country",
                f"which country has the capital {capq}",
                f"what country is {capq} the capital of",
            ]
            back_eval = [f"{capq} is the capital of where"]
            fwd_a = notes.get(q, f"The capital of {disp} is {capdisp}.")
            back_a = notes_back.get(
                capq, f"{capdisp} is the capital city of {disp}.")
        else:
            fwd_train = [
                f"what is the capital of the state of {q}",
                f"what is the capital city of the state of {q}",
                f"which city is the capital of the state of {q}",
                f"the state of {q} capital city",
            ]
            fwd_eval = [f"tell me the capital of the state of {q}"]
            back_train = [
                f"{capq} is the capital of which state",
                f"which state has the capital {capq}",
                f"what state is {capq} the capital of",
            ]
            back_eval = [f"{capq} is the capital city of which state"]
            fwd_a = notes.get(q, f"The capital of {disp} is {capdisp}.")
            back_a = notes_back.get(
                capq, f"{capdisp} is the capital city of {disp}.")
        out.append(F(topic, fwd_a, fwd_train, fwd_eval))
        out.append(F(topic, back_a, back_train, back_eval))
    return out


# ------------------------------------------------------------ world geography
def _world():
    o = _capitals(WORLD, COUNTRIES, CAPITAL_NOTE, CAPITAL_NOTE_BACK, "country")
    a = o.append

    a(F(WORLD, "There are seven continents on Earth.",
        ["how many continents are there", "how many continents are on earth",
         "number of continents"],
        ["how many continents does the world have"]))
    a(F(WORLD, "The seven continents are Africa, Antarctica, Asia, Australia, "
               "Europe, North America and South America.",
        ["what are the seven continents", "name the continents",
         "can you list the continents"],
        ["tell me all the continents"]))
    a(F(WORLD, "Asia is the biggest continent. More people live in Asia than "
               "on any other continent.",
        ["what is the biggest continent", "which continent is the largest",
         "biggest continent in the world"],
        ["which continent is the biggest one"]))
    a(F(WORLD, "Australia is the smallest continent.",
        ["what is the smallest continent", "which continent is the smallest",
         "smallest continent in the world"],
        ["which continent is tiniest"]))
    a(F(WORLD, "Antarctica is the coldest continent. It is covered in thick "
               "ice and almost nobody lives there.",
        ["what is the coldest continent", "which continent is the coldest",
         "coldest place on earth"],
        ["where is the coldest continent"]))
    a(F(WORLD, "There are five oceans on Earth.",
        ["how many oceans are there", "how many oceans are on earth",
         "number of oceans"],
        ["how many oceans does the world have"]))
    a(F(WORLD, "The five oceans are the Pacific, the Atlantic, the Indian, "
               "the Southern and the Arctic.",
        ["what are the five oceans", "name the oceans", "list the oceans"],
        ["tell me all the oceans"]))
    a(F(WORLD, "The Pacific Ocean is the biggest ocean in the world.",
        ["what is the biggest ocean", "which ocean is the largest",
         "biggest ocean in the world"],
        ["which ocean is the biggest one"]))
    a(F(WORLD, "The Arctic Ocean is the smallest ocean and the coldest one.",
        ["what is the smallest ocean", "which ocean is the smallest",
         "coldest ocean"],
        ["which ocean is tiniest"]))
    a(F(WORLD, "The Atlantic Ocean is between the Americas and Europe and "
               "Africa.",
        ["where is the atlantic ocean", "what is the atlantic ocean",
         "which ocean is between america and europe"],
        ["tell me about the atlantic ocean"]))
    a(F(WORLD, "Ocean water is salty, so it is not safe to drink.",
        ["is ocean water salty", "why is the sea salty",
         "can you drink ocean water"],
        ["is sea water salty or sweet"]))

    # ---- landmarks
    a(F(WORLD, "The Eiffel Tower is a tall iron tower in Paris, France.",
        ["where is the eiffel tower", "what country is the eiffel tower in",
         "what is the eiffel tower"],
        ["which city has the eiffel tower"]))
    a(F(WORLD, "The Great Wall of China is a very long wall in China. People "
               "built it long ago to protect the country.",
        ["where is the great wall", "what is the great wall of china",
         "what country has the great wall"],
        ["tell me about the great wall of china"]))
    a(F(WORLD, "The pyramids are in Egypt, in Africa. They were built as "
               "tombs for the pharaohs a very long time ago.",
        ["where are the pyramids", "what country has the pyramids",
         "what are the pyramids"],
        ["which country are the pyramids in"]))
    a(F(WORLD, "The Taj Mahal is a beautiful white marble building in India.",
        ["where is the taj mahal", "what country is the taj mahal in",
         "what is the taj mahal"],
        ["which country has the taj mahal"]))
    a(F(WORLD, "Big Ben is the great bell in a famous clock tower in London, "
               "England.",
        ["what is big ben", "where is big ben",
         "what city is big ben in"],
        ["which country has big ben"]))
    a(F(WORLD, "The Colosseum is a huge old stone arena in Rome, Italy.",
        ["what is the colosseum", "where is the colosseum",
         "what country is the colosseum in"],
        ["which city has the colosseum"]))
    a(F(WORLD, "The Sydney Opera House is in Sydney, Australia. Its roof "
               "looks like white sails.",
        ["where is the sydney opera house", "what is the sydney opera house",
         "what country is the sydney opera house in"],
        ["which country has the opera house with sails"]))
    a(F(WORLD, "The Leaning Tower of Pisa is in Pisa, Italy. It leans to one "
               "side because the soft ground sank under it.",
        ["where is the leaning tower of pisa", "what is the leaning tower",
         "why does the tower of pisa lean"],
        ["which country has the leaning tower"]))
    a(F(WORLD, "Machu Picchu is an old Inca city high in the mountains of "
               "Peru.",
        ["where is machu picchu", "what is machu picchu",
         "what country is machu picchu in"],
        ["which country has machu picchu"]))
    a(F(WORLD, "Stonehenge is a ring of giant standing stones in England.",
        ["what is stonehenge", "where is stonehenge",
         "what country is stonehenge in"],
        ["which country has stonehenge"]))
    a(F(WORLD, "Christ the Redeemer is a giant statue standing above Rio de "
               "Janeiro in Brazil.",
        ["what is christ the redeemer", "where is christ the redeemer",
         "what country has the big statue on the mountain"],
        ["which country has christ the redeemer"]))
    a(F(WORLD, "The Great Barrier Reef is a huge coral reef in the sea near "
               "Australia. It is the biggest coral reef in the world.",
        ["what is the great barrier reef", "where is the great barrier reef",
         "what country is the great barrier reef near"],
        ["which country has the great barrier reef"]))
    a(F(WORLD, "Niagara Falls is a giant waterfall on the border between the "
               "United States and Canada.",
        ["where is niagara falls", "what is niagara falls",
         "which countries share niagara falls"],
        ["what country is niagara falls in"]))

    # ---- rivers and mountains
    a(F(WORLD, "The Nile in Africa is usually called the longest river in the "
               "world.",
        ["what is the longest river in the world", "which river is longest",
         "longest river on earth"],
        ["tell me the longest river in the world"]))
    a(F(WORLD, "The Nile River flows through Africa, including Egypt.",
        ["where is the nile river", "what continent is the nile in",
         "which country does the nile flow through"],
        ["tell me about the nile river"]))
    a(F(WORLD, "The Amazon River is in South America. It carries more water "
               "than any other river.",
        ["where is the amazon river", "what continent is the amazon river in",
         "what is the amazon river"],
        ["which continent has the amazon river"]))
    a(F(WORLD, "The Amazon rainforest is in South America. It is the biggest "
               "rainforest in the world.",
        ["where is the amazon rainforest", "what is the amazon rainforest",
         "biggest rainforest in the world"],
        ["which continent has the amazon rainforest"]))
    a(F(WORLD, "The Yangtze River is the longest river in Asia and it flows "
               "through China.",
        ["where is the yangtze river", "what country is the yangtze in",
         "longest river in asia"],
        ["which country has the yangtze river"]))
    a(F(WORLD, "The Ganges River flows through India.",
        ["where is the ganges river", "what country is the ganges in",
         "which country has the ganges"],
        ["tell me about the ganges river"]))
    a(F(WORLD, "The Thames is the river that runs through London, England.",
        ["what river runs through london", "where is the river thames",
         "what is the thames"],
        ["which city does the thames flow through"]))
    a(F(WORLD, "The Danube flows through many countries in Europe, including "
               "Austria, Hungary and Germany.",
        ["where is the danube river", "what continent is the danube in",
         "what is the danube"],
        ["which continent has the danube river"]))
    a(F(WORLD, "Mount Everest is the tallest mountain in the world. It sits "
               "on the border between Nepal and China.",
        ["what is the tallest mountain in the world",
         "where is mount everest", "which mountain is the highest"],
        ["tell me the tallest mountain on earth"]))
    a(F(WORLD, "The Himalayas are a huge range of mountains in Asia. Mount "
               "Everest is one of them.",
        ["what are the himalayas", "where are the himalayas",
         "what continent has the himalayas"],
        ["tell me about the himalaya mountains"]))
    a(F(WORLD, "Mount Kilimanjaro is the tallest mountain in Africa, and it "
               "is in Tanzania.",
        ["what is the tallest mountain in africa", "where is kilimanjaro",
         "what is mount kilimanjaro"],
        ["which continent has mount kilimanjaro"]))
    a(F(WORLD, "The Alps are snowy mountains in Europe. They run through "
               "countries like Switzerland, France and Italy.",
        ["what are the alps", "where are the alps",
         "what continent has the alps"],
        ["tell me about the alps mountains"]))
    a(F(WORLD, "The Andes are a long chain of mountains down the west side of "
               "South America.",
        ["what are the andes", "where are the andes mountains",
         "what continent has the andes"],
        ["tell me about the andes"]))
    a(F(WORLD, "Mount Fuji is a beautiful snow topped volcano in Japan.",
        ["what is mount fuji", "where is mount fuji",
         "what country has mount fuji"],
        ["which country is mount fuji in"]))
    a(F(WORLD, "The Sahara is a giant hot desert in the north of Africa. It "
               "is the biggest hot desert in the world.",
        ["what is the sahara", "where is the sahara desert",
         "biggest hot desert in the world"],
        ["which continent has the sahara desert"]))
    a(F(WORLD, "A desert is a place that gets very little rain. Some deserts "
               "are hot and sandy and some are cold.",
        ["what is a desert", "what does desert mean",
         "tell me what a desert is"],
        ["can you explain a desert"]))
    a(F(WORLD, "An island is land with water all the way around it.",
        ["what is an island", "what does island mean",
         "tell me what an island is"],
        ["can you explain an island"]))
    a(F(WORLD, "A mountain is land that rises very high above the ground "
               "around it. A small one is called a hill.",
        ["what is a mountain", "what does mountain mean",
         "tell me what a mountain is"],
        ["can you explain what a mountain is"]))
    a(F(WORLD, "A river is fresh water that flows downhill across the land, "
               "usually all the way to the sea.",
        ["what is a river", "what does river mean",
         "tell me what a river is"],
        ["can you explain what a river is"]))
    a(F(WORLD, "A country is an area of land with its own government, its own "
               "flag and its own laws.",
        ["what is a country", "what does country mean",
         "tell me what a country is"],
        ["can you explain what a country is"]))
    a(F(WORLD, "A continent is one of the seven huge pieces of land on Earth, "
               "like Africa or Asia.",
        ["what is a continent", "what does continent mean",
         "tell me what a continent is"],
        ["can you explain what a continent is"]))

    # ---- where animals live
    for qa, aa in [
        ("kangaroos", "Kangaroos live in Australia."),
        ("koalas", "Koalas live in Australia and they eat eucalyptus leaves."),
        ("giant pandas", "Giant pandas live in the bamboo forests of China."),
        ("penguins", "Most penguins live in the cold south, and many live in "
                     "Antarctica. No wild penguins live at the North Pole."),
        ("polar bears", "Polar bears live in the cold Arctic in the far north."),
        ("lions", "Wild lions live in Africa, mostly on the grassy plains."),
        ("giraffes", "Giraffes live in Africa."),
        ("elephants", "Elephants live in Africa and in Asia."),
        ("tigers", "Wild tigers live in Asia."),
        ("kiwi birds", "Kiwi birds live in New Zealand and they cannot fly."),
        ("camels", "Camels live in deserts in Africa and Asia."),
        ("llamas", "Llamas live in South America, high up in the Andes."),
        ("sloths", "Sloths live in the rainforests of Central and South "
                   "America."),
        ("gorillas", "Wild gorillas live in the forests of Africa."),
        ("moose", "Moose live in cold northern forests in places like Canada "
                  "and Alaska."),
        ("reindeer", "Reindeer live in the cold far north of Europe, Asia and "
                     "North America."),
    ]:
        a(F(WORLD, aa,
            [f"where do {qa} live", f"what country do {qa} live in",
             f"where are {qa} from"],
            [f"what part of the world do {qa} come from"]))
    return o


SECTIONS = [_world]


def build():
    out = []
    for s in SECTIONS:
        out.extend(s())
    return out


if __name__ == "__main__":
    from tools.braino.facts import report
    report("world", build())
