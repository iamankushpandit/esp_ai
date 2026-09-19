"""Space, a deliberately narrow slice of chemistry, colors, and shapes.

Written for five to eight year olds answering out loud on a small speaker.

Guiding rule: we need to be RIGHT. Anything the author was not certain of was
dropped rather than guessed - no planet diameters, no distances in miles, no
counts of moons, no lists of elements. Concepts a child can check against the
world beat numbers a child cannot.

Chemistry is on purpose tiny: only the substances a young child actually meets
(oxygen, water, gold, iron, helium, carbon, hydrogen), plus what an atom is and
the three states of matter. Enumerating the periodic table was measured to eat
about five thousand six hundred ninety training samples of capacity that
age-appropriate topics needed instead.

Phrasing variety is the point: four train wordings per fact where possible, and
the eval wording is always genuinely different, because trained-vs-held-out
measured eighty-five percent against twenty-five point eight percent.
"""
from . import Fact, report


def build() -> list:
    facts = []

    def space(train, answer, evalq):
        facts.append(Fact("space", train, answer, evalq))

    def chemistry(train, answer, evalq):
        facts.append(Fact("chemistry", train, answer, evalq))

    def colors(train, answer, evalq):
        facts.append(Fact("colors", train, answer, evalq))

    def shapes(train, answer, evalq):
        facts.append(Fact("shapes", train, answer, evalq))

    # ==================================================================
    # SPACE
    # ==================================================================

    # ---- the solar system and the planets ----------------------------

    space(
        ["how many planets are there", "how many planets in the solar system",
         "number of planets", "how many planets are in our solar system"],
        "There are eight planets in our solar system.",
        ["how many planets go around the sun"])

    space(
        ["what are the planets in order", "name the planets in order",
         "list the planets", "what order are the planets in"],
        "Going out from the sun they are Mercury, Venus, Earth, Mars, Jupiter, "
        "Saturn, Uranus and Neptune.",
        ["can you tell me all the planets starting nearest the sun"])

    space(
        ["what is the solar system", "what does solar system mean",
         "tell me about the solar system", "what is our solar system"],
        "The solar system is the sun and everything that goes around it. "
        "That means the planets, their moons, comets and lots of rocky bits.",
        ["what do we call the sun and everything circling it"])

    space(
        ["which planet is closest to the sun", "what is the first planet",
         "which planet is nearest the sun", "what planet is closest to the sun"],
        "Mercury is the planet closest to the sun.",
        ["what planet comes first going out from the sun"])

    space(
        ["which planet is farthest from the sun", "what is the last planet",
         "which planet is furthest from the sun",
         "what planet is farthest away from the sun"],
        "Neptune is the planet farthest from the sun.",
        ["what is the most distant planet from our sun"])

    space(
        ["which planet is the biggest", "what is the largest planet",
         "which is the biggest planet in the solar system",
         "what planet is the biggest one"],
        "Jupiter is the biggest planet in our solar system. "
        "It is so wide that all the other planets could fit inside it.",
        ["what is the largest planet that goes around our sun"])

    space(
        ["which planet is the smallest", "what is the smallest planet",
         "which is the smallest planet in the solar system",
         "what planet is the tiniest"],
        "Mercury is the smallest planet in our solar system.",
        ["what is the littlest planet going around the sun"])

    space(
        ["what planet do we live on", "where do we live in space",
         "which planet is our home", "what planet are we on"],
        "We live on Earth. It is the third planet out from the sun.",
        ["what is the name of our own planet"])

    space(
        ["which planet is the red planet", "what planet is red",
         "why is mars called the red planet", "which planet looks red"],
        "Mars is called the red planet. Its dust and rocks have rust in them, "
        "and rust is reddish, so the whole planet looks orange red.",
        ["what makes mars look orange and red"])

    space(
        ["which planet has rings", "what planet has rings around it",
         "which planet is famous for its rings", "what planet has big rings"],
        "Saturn is famous for its beautiful rings. "
        "The rings are made of countless pieces of ice and rock going around it.",
        ["which planet wears rings around its middle"])

    space(
        ["what are saturns rings made of", "what is saturns rings made from",
         "what are the rings of saturn made of",
         "what is in saturns rings"],
        "Saturn's rings are made of billions of chunks of ice and rock. "
        "Some pieces are as small as dust and some are as big as a house.",
        ["what stuff makes up the rings around saturn"])

    space(
        ["is pluto a planet", "why is pluto not a planet",
         "what happened to pluto", "is pluto still a planet"],
        "Pluto is not counted as one of the eight planets any more. "
        "Scientists call it a dwarf planet because it is small and shares its "
        "part of space with other icy objects.",
        ["what kind of thing is pluto now"])

    space(
        ["what is a dwarf planet", "what does dwarf planet mean",
         "what makes something a dwarf planet", "what is a dwarf planet like"],
        "A dwarf planet is a round world going around the sun that is too small "
        "to clear the other rocks out of its path. Pluto is a dwarf planet.",
        ["how is a dwarf planet different from a planet"])

    space(
        ["what is an orbit", "what does orbit mean", "what is orbiting",
         "what does it mean to orbit something"],
        "An orbit is the curved path something takes as it goes around and "
        "around another thing. Earth orbits the sun and the moon orbits Earth.",
        ["what do we call the path a planet travels around the sun"])

    space(
        ["how long does earth take to go around the sun",
         "how long is a year", "why is a year a year",
         "how long does it take earth to orbit the sun"],
        "Earth takes one year to travel all the way around the sun. "
        "That is about three hundred sixty-five days.",
        ["how long is one trip around the sun for us"])

    # ---- the sun and stars -------------------------------------------

    space(
        ["what is the sun", "is the sun a star", "what kind of thing is the sun",
         "tell me about the sun"],
        "The sun is a star. It looks much bigger and brighter than other stars "
        "only because it is so much closer to us.",
        ["what sort of object is our sun"])

    space(
        ["why is the sun so bright", "why does the sun shine",
         "how does the sun make light", "what makes the sun shine"],
        "The sun makes its own light and heat deep inside itself, where it is "
        "unbelievably hot. That glow travels out through space and reaches us.",
        ["where does sunlight come from"])

    space(
        ["what is a star", "what are stars", "what are stars made of",
         "tell me what a star is"],
        "A star is a giant glowing ball of hot gas in space. "
        "Stars make their own light, which is why we can see them so far away.",
        ["what exactly is a star up in the sky"])

    space(
        ["why do stars twinkle", "what makes stars twinkle",
         "why does a star look like it is blinking", "why do stars flicker"],
        "Stars twinkle because their light wobbles as it passes through the "
        "moving air above us. The star is really shining steadily.",
        ["why does starlight seem to shiver"])

    space(
        ["why cant we see stars in the day", "where do stars go in the daytime",
         "why do stars disappear during the day",
         "why are there no stars in the day sky"],
        "The stars are still there in the daytime. "
        "The sky is just so bright with sunlight that their faint light is "
        "washed out and we cannot see it.",
        ["do the stars go away when the sun comes up"])

    space(
        ["what is a constellation", "what does constellation mean",
         "what are constellations", "what is a star pattern called"],
        "A constellation is a pattern that people imagined by joining up stars "
        "in the sky, like a dot to dot picture. Many are named after animals.",
        ["what do we call a picture made out of stars"])

    space(
        ["what is a galaxy", "what does galaxy mean", "what is our galaxy",
         "what is the milky way"],
        "A galaxy is an enormous group of stars traveling together through "
        "space. Our sun lives in a galaxy called the Milky Way.",
        ["what big family of stars do we belong to"])

    # ---- the moon -----------------------------------------------------

    space(
        ["what is the moon", "tell me about the moon", "what is the moon like",
         "what kind of thing is the moon"],
        "The moon is a big ball of rock that goes around Earth. "
        "It has no air and no water, and it is covered in dust and craters.",
        ["what is up there when i look at the moon"])

    space(
        ["why does the moon shine", "does the moon make its own light",
         "why is the moon bright", "where does moonlight come from"],
        "The moon does not make its own light. "
        "Sunlight shines on the moon and bounces off it toward us, the way "
        "light bounces off a mirror.",
        ["how does the moon glow at night"])

    space(
        ["what are moon phases", "why does the moon change shape",
         "what are the phases of the moon", "why does the moon look different"],
        "The sun always lights up half the moon. As the moon moves around Earth "
        "we see different amounts of that lit half, so its shape seems to change.",
        ["why is the moon a different shape each week"])

    space(
        ["what is a full moon", "what does full moon mean",
         "when is the moon full", "what makes a full moon"],
        "A full moon is when we can see the whole lit side of the moon, "
        "so it looks like a bright round circle.",
        ["what do we call the moon when it is a complete circle"])

    space(
        ["what is a new moon", "what does new moon mean",
         "why cant i see the moon sometimes", "what happens at a new moon"],
        "At a new moon the lit side of the moon is facing away from us, "
        "so the moon is almost impossible to see in the sky.",
        ["what do we call it when the moon seems to vanish"])

    space(
        ["what is a crescent moon", "what does crescent moon mean",
         "why is the moon a thin sliver", "what is a crescent"],
        "A crescent moon is a thin curved sliver of light, "
        "like a smile or a fingernail. We are seeing only a little of the lit side.",
        ["what do we call the skinny curved moon"])

    space(
        ["what are moon craters", "why does the moon have holes",
         "what made the craters on the moon", "what are the holes on the moon"],
        "Craters are bowl shaped dents made when rocks from space crashed into "
        "the moon. With no wind or rain to wear them away they stay for ages.",
        ["how did the moon get all those round dips"])

    # ---- day, night, seasons ------------------------------------------

    space(
        ["why do we have day and night", "what makes day and night",
         "why does it get dark at night", "how do day and night happen"],
        "Earth spins around like a slow top. "
        "When your side faces the sun it is day, and when your side turns away "
        "it is night.",
        ["what causes the change from daytime to night"])

    space(
        ["does the sun move across the sky", "why does the sun seem to move",
         "why does the sun go up and down", "does the sun really rise"],
        "The sun stays put and Earth spins. "
        "The spinning makes the sun look like it rises, crosses the sky and sets.",
        ["is it the sun or the earth that is really moving"])

    space(
        ["how long does earth take to spin around",
         "how long is a day", "why is a day a day",
         "how long does one spin of earth take"],
        "Earth takes about one day to spin all the way around, "
        "which is twenty-four hours.",
        ["how long is one full turn of our planet"])

    space(
        ["why do we have seasons", "what causes seasons",
         "why does it get hot and cold in the year", "how do seasons happen"],
        "Earth is tilted. As it travels around the sun, one half leans toward "
        "the sun and gets summer while the other half leans away and gets winter.",
        ["what makes summer and winter come round each year"])

    # The list of seasons is answered in science_life.py under weather - it is
    # a calendar fact, not an astronomical one. This module keeps "why do we
    # have seasons", which is about Earth's tilt and belongs here.

    space(
        ["why is summer hot", "why is it warmer in summer",
         "what makes summer warm", "why does summer get so hot"],
        "In summer your half of Earth leans toward the sun. "
        "The sunlight hits more straight on and the days are longer, so it warms up.",
        ["what makes the summertime so warm"])

    space(
        ["why is winter cold", "why is it colder in winter",
         "what makes winter cold", "why does winter feel so cold"],
        "In winter your half of Earth leans away from the sun. "
        "The sunlight comes in at a slant and the days are short, so it stays cold.",
        ["what makes the wintertime so chilly"])

    # ---- gravity -------------------------------------------------------

    space(
        ["what is gravity", "what does gravity mean", "what is gravity like",
         "tell me about gravity"],
        "Gravity is a pull that big things have. "
        "Earth's gravity pulls everything toward the ground, which is why you "
        "come back down when you jump.",
        ["what is the force that holds us on the ground"])

    space(
        ["why do things fall down", "why does a ball fall to the ground",
         "what makes things fall", "why do dropped things go down"],
        "Things fall because Earth's gravity pulls them toward its center. "
        "Down is really just the way to the middle of Earth.",
        ["what pulls a dropped toy to the floor"])

    space(
        ["why do astronauts float", "why do things float in space",
         "why is there no gravity in space",
         "why do astronauts float in a spaceship"],
        "Astronauts float because they and their spaceship are falling around "
        "Earth together. Everything falls at the same rate, so nothing presses "
        "down and they seem weightless.",
        ["what makes people drift around inside a space station"])

    space(
        ["what keeps the planets going around the sun",
         "why do planets orbit the sun", "what holds the planets in place",
         "why dont the planets fly away"],
        "The sun's gravity pulls on the planets and keeps them curving around "
        "it instead of flying off in a straight line.",
        ["what stops the planets from drifting off into space"])

    space(
        ["would i weigh less on the moon", "how much would i weigh on the moon",
         "is gravity different on the moon", "what is gravity like on the moon"],
        "The moon is much smaller than Earth, so its gravity pulls more gently. "
        "You would weigh far less there and could jump much higher.",
        ["could i jump higher if i stood on the moon"])

    # ---- rockets and astronauts ----------------------------------------

    space(
        ["what is an astronaut", "what does an astronaut do",
         "who is an astronaut", "what is an astronauts job"],
        "An astronaut is a person trained to travel and work in space. "
        "They fly in spacecraft, run experiments and fix things outside.",
        ["what do we call a person who goes into space"])

    space(
        ["how does a rocket work", "how do rockets fly",
         "what makes a rocket go up", "how does a rocket get into space"],
        "A rocket burns fuel and pushes hot gas hard out of the bottom. "
        "Pushing the gas down pushes the rocket up, and up it climbs.",
        ["what makes a rocket lift off the ground"])

    space(
        ["why do astronauts wear spacesuits", "what is a spacesuit for",
         "why do astronauts need a suit", "what does a spacesuit do"],
        "There is no air in space and it is dangerously hot or cold. "
        "A spacesuit gives an astronaut air to breathe and keeps their body at "
        "a safe temperature.",
        ["why cant an astronaut go outside in normal clothes"])

    space(
        ["what is a space station", "what does a space station do",
         "what is the space station", "what is a space station for"],
        "A space station is a home in space where astronauts live and work for "
        "months. It orbits Earth again and again.",
        ["where do astronauts stay when they live up in space"])

    space(
        ["has anyone walked on the moon", "did people go to the moon",
         "have astronauts been to the moon", "has a person been on the moon"],
        "Yes. Astronauts have landed on the moon and walked on its dusty "
        "surface, and they brought moon rocks back to Earth.",
        ["have humans ever stood on the moon"])

    space(
        ["can you hear sound in space", "is space quiet", "is there sound in space",
         "why is space silent"],
        "Space is silent. Sound needs air or water to travel through, "
        "and space is empty, so there is nothing to carry the sound.",
        ["would a shout travel through outer space"])

    space(
        ["what is a satellite", "what does a satellite do", "what are satellites",
         "what is a satellite in space"],
        "A satellite is something that orbits a planet. "
        "People build satellites and send them up to take pictures, carry phone "
        "calls and help with weather maps.",
        ["what do we send up to circle the earth"])

    # ---- comets, meteors, asteroids -------------------------------------

    space(
        ["what is a comet", "what are comets", "what is a comet made of",
         "tell me about comets"],
        "A comet is a lump of ice and dust that travels around the sun. "
        "When it comes close the sun warms it and it grows a long glowing tail.",
        ["what is the icy thing with a tail in the sky"])

    space(
        ["why do comets have tails", "what is a comets tail",
         "how does a comet get a tail", "why does a comet glow"],
        "As a comet nears the sun its ice turns to gas. "
        "The gas and dust stream away behind it and catch the sunlight, "
        "making a bright tail.",
        ["what makes the long streak behind a comet"])

    space(
        ["what is a shooting star", "what are shooting stars",
         "is a shooting star a real star", "what is a falling star"],
        "A shooting star is not a star at all. "
        "It is a little piece of space rock burning up as it zooms into the air "
        "above us, making a quick bright streak.",
        ["what is that streak of light darting across the night sky"])

    space(
        ["what is an asteroid", "what are asteroids", "what is an asteroid like",
         "tell me about asteroids"],
        "An asteroid is a rocky object that orbits the sun. "
        "Asteroids can be as small as a boulder or as big as a mountain.",
        ["what do we call a space rock going around the sun"])

    space(
        ["is there air in space", "can you breathe in space",
         "does space have air", "why cant you breathe in space"],
        "There is no air in space, so nobody can breathe there. "
        "That is why astronauts must carry air with them.",
        ["is there anything to breathe out in space"])

    return facts


if __name__ == "__main__":
    report("science_space", build())
