"""Animals, the body, plants, and weather.

These were the starved topics. In the previous corpus the body had 140 samples
and animals 569, against 73,687 for arithmetic - a ratio of over five hundred
to one. Balancing could not fix that, because balancing repeats what is there
and there was almost nothing there to repeat: "body" hit the repeat cap at
twelve copies of the same handful of facts. The fix is more facts, not more
copies of the same ones.

Health and body answers stay gentle and concrete. Nothing here should worry a
child who asks about bones or hearts at bedtime.
"""
from . import Fact, report


def F(topic, answer, train, evalq):
    return Fact(topic=topic, train=list(train), evalq=list(evalq), answer=answer)


ANI = "animals"
BODY = "the body"
PLANT = "plants & nature"
WEA = "weather"


# ---------------------------------------------------------------- animals
# (adult, baby) - generated both directions, so the model sees the mapping
# as a rule rather than as a one-way lookup.
_BABIES = [
    ("dog", "puppy", "A puppy"), ("cat", "kitten", "A kitten"),
    ("cow", "calf", "A calf"), ("sheep", "lamb", "A lamb"),
    ("horse", "foal", "A foal"), ("duck", "duckling", "A duckling"),
    ("goose", "gosling", "A gosling"), ("chicken", "chick", "A chick"),
    ("pig", "piglet", "A piglet"), ("goat", "kid", "A kid"),
    ("frog", "tadpole", "A tadpole"), ("bear", "cub", "A cub"),
    ("lion", "cub", "A cub"), ("kangaroo", "joey", "A joey"),
    ("deer", "fawn", "A fawn"), ("swan", "cygnet", "A cygnet"),
]

# (animal, sound) - "what sound does a X make"
_SOUNDS = [
    ("dog", "barks"), ("cat", "meows"), ("cow", "moos"), ("sheep", "baas"),
    ("horse", "neighs"), ("duck", "quacks"), ("pig", "oinks"),
    ("lion", "roars"), ("owl", "hoots"), ("bee", "buzzes"),
    ("frog", "croaks"), ("mouse", "squeaks"), ("wolf", "howls"),
    ("snake", "hisses"),
]

_ANIMALS = [
    ("Biggest animal",
     "The blue whale is the biggest animal on Earth. It is bigger than any "
     "dinosaur was, and it lives in the ocean.",
     ["what is the biggest animal", "which animal is the largest",
      "what is the largest animal in the world", "name the biggest animal"],
     ["which is the biggest animal there is"]),
    ("Biggest land animal",
     "The African elephant is the biggest animal that lives on land.",
     ["what is the biggest land animal", "which is the largest animal on land",
      "what is the biggest animal that walks", "name the biggest land animal"],
     ["which animal on land is largest"]),
    ("Fastest animal on land",
     "The cheetah is the fastest animal on land. It can run faster than a car "
     "drives in town, but only for a short burst.",
     ["what is the fastest animal", "which animal runs fastest",
      "what is the fastest land animal", "name the fastest animal"],
     ["which animal can run the quickest"]),
    ("Tallest animal",
     "The giraffe is the tallest animal. Its long neck lets it eat leaves at "
     "the top of trees.",
     ["what is the tallest animal", "which animal is tallest",
      "what animal has a long neck", "name the tallest animal"],
     ["which animal grows the tallest"]),
    ("Mammals",
     "Mammals have fur or hair, and mothers feed their babies milk. Dogs, cats, "
     "whales and people are all mammals.",
     ["what is a mammal", "what are mammals", "tell me about mammals",
      "am i a mammal"],
     ["what makes an animal a mammal"]),
    ("Birds",
     "Birds have feathers, wings and a beak, and they lay eggs. Most birds can "
     "fly, but a few like penguins and ostriches cannot.",
     ["what is a bird", "what are birds", "tell me about birds",
      "what makes a bird a bird"],
     ["how do you know something is a bird"]),
    ("Fish",
     "Fish live in water and breathe through gills instead of lungs. They swim "
     "with fins and most are covered in scales.",
     ["what is a fish", "tell me about fish", "how do fish breathe",
      "what do fish use to swim"],
     ["what makes an animal a fish"]),
    ("Insects",
     "An insect has six legs and three body parts. Ants, bees and beetles are "
     "insects, and most have wings.",
     ["what is an insect", "how many legs does an insect have",
      "tell me about insects", "what makes something an insect"],
     ["how many legs do insects have"]),
    ("Spiders",
     "A spider has eight legs, so it is not an insect. Spiders spin webs to "
     "catch the flies they eat.",
     ["how many legs does a spider have", "is a spider an insect",
      "tell me about spiders", "what do spiders eat"],
     ["how many legs are on a spider"]),
    ("Reptiles",
     "Reptiles have dry scaly skin and most lay eggs. Snakes, lizards, turtles "
     "and crocodiles are reptiles.",
     ["what is a reptile", "what are reptiles", "tell me about reptiles",
      "name some reptiles"],
     ["what makes an animal a reptile"]),
    ("Amphibians",
     "Amphibians live partly in water and partly on land. Frogs and newts start "
     "life in water and grow legs later.",
     ["what is an amphibian", "tell me about amphibians", "what are amphibians",
      "name some amphibians"],
     ["what makes an animal an amphibian"]),
    ("Herbivore",
     "A herbivore is an animal that eats only plants. Cows, rabbits and horses "
     "are herbivores.",
     ["what is a herbivore", "what animals eat only plants",
      "tell me about herbivores", "what does herbivore mean"],
     ["which animals are herbivores"]),
    ("Carnivore",
     "A carnivore is an animal that eats meat. Lions, wolves and eagles are "
     "carnivores.",
     ["what is a carnivore", "what animals eat meat", "tell me about carnivores",
      "what does carnivore mean"],
     ["which animals are carnivores"]),
    ("Omnivore",
     "An omnivore eats both plants and meat. Bears, pigs and people are "
     "omnivores.",
     ["what is an omnivore", "what animals eat everything",
      "tell me about omnivores", "what does omnivore mean"],
     ["which animals are omnivores"]),
    ("Habitat",
     "A habitat is the place an animal lives, where it can find food and "
     "shelter. A pond, a forest and a desert are all habitats.",
     ["what is a habitat", "what does habitat mean", "tell me about habitats",
      "where do animals live"],
     ["can you explain what a habitat is"]),
    ("Hibernation",
     "Hibernating is a deep winter sleep. Some animals eat a lot in autumn and "
     "then sleep through the cold, when food is hard to find.",
     ["what is hibernation", "what does hibernate mean",
      "which animals sleep all winter", "tell me about hibernating"],
     ["can you explain hibernation"]),
    ("Migration",
     "Migrating means travelling a long way when the seasons change. Many birds "
     "fly somewhere warm for the winter and come back in spring.",
     ["what is migration", "why do birds fly south", "what does migrate mean",
      "tell me about migration"],
     ["can you explain why animals migrate"]),
    ("Camouflage",
     "Camouflage is when an animal's colors help it hide. A green frog on a "
     "green leaf is hard for a hungry bird to spot.",
     ["what is camouflage", "how do animals hide", "what does camouflage mean",
      "tell me about camouflage"],
     ["can you explain camouflage"]),
    ("Nocturnal",
     "Nocturnal animals are awake at night and sleep in the day. Owls, bats and "
     "hedgehogs are nocturnal.",
     ["what does nocturnal mean", "which animals come out at night",
      "what is a nocturnal animal", "tell me about nocturnal animals"],
     ["which animals are awake at night"]),
    ("Bats",
     "A bat is the only mammal that really flies. Bats hunt at night and find "
     "their way using sounds too high for us to hear.",
     ["what is a bat", "can bats fly", "tell me about bats",
      "is a bat a bird"],
     ["are bats birds or mammals"]),
    ("Penguins",
     "Penguins are birds that cannot fly. They use their wings as flippers and "
     "are wonderful swimmers.",
     ["can penguins fly", "tell me about penguins", "are penguins birds",
      "how do penguins swim"],
     ["why can penguins not fly"]),
    ("Elephant",
     "An elephant is huge and gray with big ears and a long trunk. It uses the "
     "trunk to drink, to grab food and to give itself a shower.",
     ["tell me about elephants", "what is an elephant",
      "what does an elephant use its trunk for", "why do elephants have trunks"],
     ["what is an elephant's trunk for"]),
    ("Butterfly life cycle",
     "A butterfly starts as an egg, hatches into a caterpillar, makes a hard "
     "case called a chrysalis, and comes out as a butterfly.",
     ["how does a caterpillar become a butterfly",
      "what is the life cycle of a butterfly", "tell me about butterflies",
      "where do butterflies come from"],
     ["how do butterflies grow up"]),
    ("Bees",
     "Bees collect nectar from flowers to make honey. While they visit, they "
     "carry pollen from flower to flower, which helps new plants grow.",
     ["what do bees do", "how do bees make honey", "tell me about bees",
      "why are bees important"],
     ["why do bees matter so much"]),
    ("Pets",
     "A pet is an animal that lives with people and is looked after. Dogs, cats, "
     "rabbits, fish and hamsters are common pets.",
     ["what is a pet", "name some pets", "what animals are pets",
      "tell me about pets"],
     ["which animals do people keep as pets"]),
    ("Farm animals",
     "Farm animals are kept for food and work. Cows, sheep, pigs, chickens, "
     "horses and goats all live on farms.",
     ["what animals live on a farm", "name some farm animals",
      "tell me about farm animals", "what is a farm animal"],
     ["which animals are on a farm"]),
    ("Where cows milk comes from",
     "Milk comes from cows. Farmers milk them, and the milk goes to shops in "
     "bottles and cartons.",
     ["where does milk come from", "what animal gives us milk",
      "how do we get milk", "which animal makes milk for us"],
     ["what animal does our milk come from"]),
    ("Eggs",
     "Most eggs we eat come from chickens. A hen lays them, and farmers collect "
     "them every day.",
     ["where do eggs come from", "what animal lays the eggs we eat",
      "which animal gives us eggs", "how do we get eggs"],
     ["what animal do our eggs come from"]),
    ("Wool",
     "Wool comes from sheep. Their thick coat is cut off in summer, which does "
     "not hurt them, and it is spun into yarn for jumpers and socks.",
     ["where does wool come from", "what animal gives us wool",
      "how do we get wool", "which animal makes wool"],
     ["what animal does wool come from"]),
]

# ---------------------------------------------------------------- the body
_BODY = [
    ("Five senses",
     "You have five senses. They are seeing, hearing, smelling, tasting and "
     "touching.",
     ["what are the five senses", "how many senses do i have",
      "name the senses", "what are my senses"],
     ["can you list the five senses"]),
    ("Eyes",
     "Your eyes let you see. They take in light and send pictures to your brain.",
     ["what do my eyes do", "what are eyes for", "how do i see",
      "which part of me sees"],
     ["what is the job of my eyes"]),
    ("Ears",
     "Your ears let you hear. They catch sounds in the air and turn them into "
     "messages for your brain.",
     ["what do my ears do", "what are ears for", "how do i hear",
      "which part of me hears"],
     ["what is the job of my ears"]),
    ("Nose",
     "Your nose lets you smell, and it helps you breathe. Smelling also helps "
     "you taste your food.",
     ["what does my nose do", "what is a nose for", "how do i smell things",
      "which part of me smells"],
     ["what is the job of my nose"]),
    ("Tongue",
     "Your tongue lets you taste, and it helps you talk and swallow. It can "
     "tell sweet, salty, sour and bitter.",
     ["what does my tongue do", "what is a tongue for", "how do i taste",
      "which part of me tastes"],
     ["what is the job of my tongue"]),
    ("Skin",
     "Your skin covers your whole body. It lets you feel touch, keeps germs "
     "out, and holds everything in.",
     ["what does my skin do", "what is skin for", "how do i feel things",
      "why do i have skin"],
     ["what is the job of my skin"]),
    ("Heart",
     "Your heart is a muscle that pumps blood all around your body. You can "
     "feel it beating faster after you run.",
     ["what does my heart do", "what is the heart for", "where is my heart",
      "why does my heart beat"],
     ["what is the job of the heart"]),
    ("Lungs",
     "You have two lungs. They fill with air when you breathe in and take the "
     "oxygen your body needs.",
     ["what do my lungs do", "how many lungs do i have", "what are lungs for",
      "how do i breathe"],
     ["what is the job of my lungs"]),
    ("Brain",
     "Your brain is in charge of everything. It helps you think, remember, move "
     "and feel, and it never stops working.",
     ["what does my brain do", "what is the brain for", "where is my brain",
      "how do i think"],
     ["what is the job of the brain"]),
    ("Bones",
     "Bones are hard and strong, and together they make your skeleton. They "
     "hold you up and keep the soft parts inside safe.",
     ["what do bones do", "what are bones for", "why do i have bones",
      "what is a skeleton"],
     ["what is the job of your bones"]),
    ("Number of bones",
     "A grown up has two hundred and six bones. Children have more, because "
     "some small bones join together as you grow.",
     ["how many bones are in the body", "how many bones do i have",
      "what is the number of bones in a person",
      "how many bones does a grown up have"],
     ["how many bones are there in a body"]),
    ("Muscles",
     "Muscles pull on your bones to make you move. They get stronger when you "
     "run and play.",
     ["what do muscles do", "what are muscles for", "how do i move",
      "how do muscles get stronger"],
     ["what is the job of muscles"]),
    ("Teeth",
     "Teeth bite and chew your food so you can swallow it. Brushing twice a day "
     "keeps them healthy.",
     ["what do teeth do", "what are teeth for", "why should i brush my teeth",
      "how do i keep my teeth healthy"],
     ["what is the job of teeth"]),
    ("Baby teeth",
     "Your first teeth are called baby teeth. They fall out one at a time and "
     "bigger grown up teeth come in behind them.",
     ["why do my teeth fall out", "what are baby teeth",
      "why do i lose teeth", "what happens when a tooth falls out"],
     ["why do children lose their teeth"]),
    ("Blood",
     "Blood carries oxygen and food to every part of your body. Your heart "
     "keeps it moving all the time.",
     ["what does blood do", "what is blood for", "why do i have blood",
      "what does blood carry"],
     ["what is the job of blood"]),
    ("Why we sleep",
     "Sleep gives your body and brain time to rest and grow. It also helps you "
     "remember what you learned that day.",
     ["why do i need to sleep", "what does sleep do", "why do we sleep",
      "is sleeping important"],
     ["why is sleep good for you"]),
    ("How much sleep",
     "Children your age need about ten or eleven hours of sleep each night.",
     ["how much sleep do i need", "how many hours should i sleep",
      "how long should a child sleep", "how much sleep is enough"],
     ["how many hours of sleep should i get"]),
    ("Healthy eating",
     "Eating lots of different foods keeps you strong. Fruit and vegetables "
     "every day are the most important part.",
     ["what should i eat to be healthy", "how do i eat well",
      "what is healthy food", "why should i eat vegetables"],
     ["what food keeps me healthy"]),
    ("Drinking water",
     "Your body needs water to work. Drinking water through the day stops you "
     "feeling tired and thirsty.",
     ["why should i drink water", "is water good for me",
      "why do i need water", "how much water should i drink"],
     ["why is drinking water important"]),
    ("Exercise",
     "Running, jumping and playing make your heart and muscles stronger. Moving "
     "about every day is good for you.",
     ["why should i exercise", "is playing good for me",
      "why is running good for you", "what does exercise do"],
     ["why is moving around good for me"]),
    ("Washing hands",
     "Washing your hands with soap and water gets rid of germs. Do it before "
     "you eat and after you use the toilet.",
     ["why should i wash my hands", "when should i wash my hands",
      "how do i stop germs", "why do i need soap"],
     ["why is hand washing important"]),
    ("Why we sneeze",
     "Sneezing blows out dust and tickles from your nose. It is your body "
     "cleaning itself.",
     ["why do i sneeze", "what makes me sneeze", "what is a sneeze",
      "why do people sneeze"],
     ["what causes a sneeze"]),
    ("Why we yawn",
     "People yawn when they are tired or bored. Seeing someone else yawn often "
     "makes you yawn too.",
     ["why do i yawn", "what makes me yawn", "what is a yawn",
      "why is yawning catching"],
     ["what causes a yawn"]),
    # Finger counting belongs to numbers.py (which shows the work) and
    # games_play.py (which owns counting on hands). This one keeps toes, so
    # the three modules do not answer the same question three ways.
    ("Toes",
     "You have ten toes, five on each foot, the same as your ten fingers.",
     ["how many toes do i have", "how many toes are on one foot",
      "how many toes have i got", "how many fingers and toes altogether"],
     ["what is the number of toes you have"]),
    ("Growing",
     "You grow a little bit every day, mostly while you are asleep. Eating well "
     "and sleeping enough helps you grow.",
     ["how do i grow", "why do i grow", "when do i grow",
      "what helps me grow"],
     ["what makes children grow"]),
]

# ------------------------------------------------------------- plants
_PLANTS = [
    ("What plants need",
     "Plants need four things to grow. They need water, light, air and soil to "
     "hold their roots.",
     ["what do plants need to grow", "what does a plant need",
      "how do plants grow", "what helps a plant grow"],
     ["what things must a plant have"]),
    ("Seeds",
     "A seed is a tiny package with a baby plant inside. Put it in soil and "
     "give it water, and it starts to grow.",
     ["what is a seed", "what is inside a seed", "how does a seed grow",
      "where do plants come from"],
     ["can you explain what a seed is"]),
    ("Roots",
     "Roots grow down into the soil. They drink up water and hold the plant "
     "steady so the wind cannot blow it over.",
     ["what do roots do", "what are roots for", "why do plants have roots",
      "where do roots grow"],
     ["what is the job of roots"]),
    ("Stem",
     "The stem holds the plant up and carries water from the roots to the "
     "leaves.",
     ["what does a stem do", "what is a stem for", "why do plants have stems",
      "how does water get to the leaves"],
     ["what is the job of a stem"]),
    ("Leaves",
     "Leaves catch sunlight and use it to make food for the plant. That is why "
     "plants need a sunny spot.",
     ["what do leaves do", "what are leaves for", "why do plants have leaves",
      "how do plants make food"],
     ["what is the job of a leaf"]),
    ("Flowers",
     "Flowers make seeds so new plants can grow. Their bright colors and smell "
     "bring bees and butterflies in to help.",
     ["what do flowers do", "why do plants have flowers",
      "why are flowers colorful", "what are flowers for"],
     ["what is the job of a flower"]),
    ("Trees",
     "A tree is a very big plant with a hard woody trunk. Some live for "
     "hundreds of years.",
     ["what is a tree", "tell me about trees", "how are trees different from plants",
      "how long do trees live"],
     ["what makes a tree a tree"]),
    ("Why trees matter",
     "Trees make the oxygen we breathe and give shade and homes to animals. "
     "Their roots also hold the soil in place.",
     ["why are trees important", "what do trees do for us",
      "why do we need trees", "what good are trees"],
     ["why do trees matter"]),
    ("Fruit and vegetables",
     "Fruit grows from a flower and holds the seeds, like apples and tomatoes. "
     "Vegetables are the other parts we eat, like carrots and leaves.",
     ["what is the difference between fruit and vegetables",
      "what is a fruit", "what is a vegetable", "is a tomato a fruit"],
     ["how are fruits and vegetables different"]),
    ("Where carrots grow",
     "A carrot is a root, so it grows under the ground. The leafy green top "
     "sticks up above the soil.",
     ["where do carrots grow", "do carrots grow underground",
      "what part of the plant is a carrot", "how do carrots grow"],
     ["which part of a carrot plant do we eat"]),
    ("Evergreen",
     "Evergreen trees keep their leaves all year. Pine and fir trees are "
     "evergreen, which is why they stay green in winter.",
     ["what is an evergreen tree", "which trees stay green in winter",
      "what does evergreen mean", "tell me about evergreen trees"],
     ["which trees keep their leaves all year"]),
    ("Grass",
     "Grass is a plant with long thin leaves. It grows back when it is cut, "
     "which is why lawns need mowing.",
     ["what is grass", "why does grass grow back", "tell me about grass",
      "why do we cut grass"],
     ["what kind of plant is grass"]),
]

# ------------------------------------------------------------- weather
_WEATHER = [
    ("Rain",
     "Rain is water falling from clouds. When the tiny drops in a cloud join "
     "into bigger ones, they get too heavy to float and fall down.",
     ["what is rain", "why does it rain", "where does rain come from",
      "how does rain happen"],
     ["can you explain what makes it rain"]),
    ("Clouds",
     "Clouds are made of millions of tiny water drops floating in the air. "
     "There are so many together that you can see them.",
     ["what are clouds made of", "what is a cloud", "how are clouds made",
      "why can i see clouds"],
     ["what is a cloud made from"]),
    ("Snow",
     "Snow is rain that froze before it reached the ground. Each snowflake is a "
     "tiny piece of ice with six sides.",
     ["what is snow", "why does it snow", "how is snow made",
      "when does it snow"],
     ["can you explain what snow is"]),
    ("Wind",
     "Wind is air that is moving. You cannot see it, but you can watch it move "
     "the trees and feel it on your face.",
     ["what is wind", "why is it windy", "where does wind come from",
      "what makes wind"],
     ["can you explain what wind is"]),
    ("Thunder and lightning",
     "Lightning is a huge spark in a storm cloud, and thunder is the loud bang "
     "it makes. You see the flash first because light travels faster than sound.",
     ["what is thunder", "what is lightning", "why do you see lightning first",
      "what makes thunder"],
     ["why does thunder come after the flash"]),
    ("Fog",
     "Fog is a cloud sitting on the ground. It is made of tiny water drops, and "
     "it makes everything look soft and hard to see.",
     ["what is fog", "why is it foggy", "how does fog happen",
      "what makes fog"],
     ["can you explain what fog is"]),
    ("Storms",
     "A storm is wild weather with strong wind and heavy rain. The safest place "
     "in a storm is indoors.",
     ["what is a storm", "what happens in a storm", "tell me about storms",
      "where should i go in a storm"],
     ["what should i do during a storm"]),
    ("The Sun and warmth",
     "The Sun heats the ground and the air. That is why the middle of the day "
     "is warmer than the early morning.",
     ["why is it warm in the day", "what makes it hot",
      "why is the morning cold", "what warms the air"],
     ["what makes the day warm"]),
    ("Seasons",
     "There are four seasons. They are spring, summer, autumn and winter, and "
     "they come round in that order every year.",
     ["what are the four seasons", "name the seasons", "how many seasons are there",
      "what order do the seasons come in"],
     ["can you list the seasons"]),
    # "why do we have seasons" is the Earth's tilt, which is astronomy, so it
    # is answered once in science_space.py. This module keeps the season list
    # above, which is a calendar fact rather than an astronomical one.
    ("What to wear when cold",
     "When it is cold, wear a warm coat, a hat and gloves. Layers work best, "
     "because you can take one off if you warm up.",
     ["what should i wear when it is cold", "what do i wear in winter",
      "how do i stay warm outside", "what should i wear in the snow"],
     ["what clothes are best for cold weather"]),
    ("What to wear when raining",
     "When it rains, wear a waterproof coat and boots, and take an umbrella if "
     "you have one.",
     ["what should i wear when it rains", "what do i wear in the rain",
      "how do i stay dry", "what should i take if it is raining"],
     ["what clothes are best for rain"]),
    ("Sun safety",
     "On a very sunny day wear a hat and sun cream, and drink plenty of water. "
     "Never look straight at the Sun.",
     ["how do i stay safe in the sun", "what should i wear when it is sunny",
      "why should i wear a hat in summer", "how do i not get sunburnt"],
     ["what keeps me safe on a sunny day"]),
    ("Thermometer",
     "A thermometer measures how hot or cold something is. That measurement is "
     "called the temperature.",
     ["what is a thermometer", "how do you measure temperature",
      "what measures how hot it is", "what is temperature"],
     ["what tool tells you how hot it is"]),
    ("Why puddles vanish",
     "Puddles dry up because the Sun warms the water and turns it into a gas "
     "that floats away into the air. That is called evaporating.",
     ["where do puddles go", "why do puddles dry up",
      "what is evaporation", "why does water disappear"],
     ["what happens to a puddle when it dries"]),
]


def build():
    facts = []

    for topic, table in ((ANI, _ANIMALS), (BODY, _BODY),
                         (PLANT, _PLANTS), (WEA, _WEATHER)):
        for _name, answer, train, evalq in table:
            facts.append(F(topic, answer, train, evalq))

    # Baby-animal names, generated both ways round. A lookup learned in one
    # direction only is a lookup; learned in both it starts to behave like a
    # rule the model can apply to a pair it half remembers.
    for adult, baby, disp in _BABIES:
        facts.append(F(ANI,
                       f"{disp} is a baby {adult}.",
                       [f"what is a baby {adult} called",
                        f"what do you call a baby {adult}",
                        f"name for a baby {adult}",
                        f"what is the young of a {adult} called"],
                       [f"what would you call a baby {adult}"]))

    for animal, sound in _SOUNDS:
        facts.append(F(ANI,
                       f"A {animal} {sound}.",
                       [f"what sound does a {animal} make",
                        f"what noise does a {animal} make",
                        f"what does a {animal} say",
                        f"how does a {animal} sound"],
                       [f"tell me the sound a {animal} makes"]))

    return facts


if __name__ == "__main__":
    from collections import Counter
    f = build()
    report("science_life", f)
    for t, n in Counter(x.topic for x in f).most_common():
        print(f"   {t:18s} {n}")
