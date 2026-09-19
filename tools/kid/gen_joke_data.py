"""Generate kid-safe joke Q&A for the TinyTalk 2 fine-tune.

A 19.7M model cannot invent jokes; it can only recite ones it memorised. The
problem with training bare "tell me a joke" is that it is a one-to-many map:
one prompt, hundreds of valid answers, so greedy/low-temperature decoding
collapses to the single most likely joke and the toy tells it every time.

So every joke is keyed to a distinct *subject* ("a joke about cows"), which
turns the map back into many-to-one - the shape this model is good at, and the
shape the rest of the kid data already uses. A small pool of bare triggers
("tell me a joke", "make me laugh", ...) is then partitioned across popular
jokes so that even a topic-less ask varies with the phrasing the child uses.

Output format matches gen_kid_data.py:  User: <q>\nBot: <a><|endoftext|>

  python tools/kid/gen_joke_data.py --out models_out/kid
"""
import argparse
import json
import random
from pathlib import Path

# ---------------------------------------------------------------- jokes
# (subject, joke). One joke per subject: the subject is the retrieval key, so
# duplicates would reintroduce the one-to-many problem this file exists to fix.
JOKES = [
    ("cow", "Why did the cow go to space? To see the Moooon!"),
    ("chicken", "Why did the chicken cross the playground? To get to the other slide!"),
    ("duck", "What do ducks eat with their soup? Quackers!"),
    ("bee", "What do you call a bee that can't make up its mind? A maybe!"),
    ("bear", "What do you call a bear with no teeth? A gummy bear!"),
    ("cat", "What do cats like to eat on a hot day? A mice cream cone!"),
    ("dog", "What kind of dog can tell the time? A watch dog!"),
    ("fish", "Where do fish keep their money? In the river bank!"),
    ("elephant", "Why do elephants never use computers? They're scared of the mouse!"),
    ("monkey", "What is a monkey's favorite cookie? Chocolate chimp!"),
    ("frog", "What is a frog's favorite drink? Croak a cola!"),
    ("pig", "Why did the pig get hired at the restaurant? Because he was really good at bacon!"),
    ("sheep", "What do you call a sheep with no legs? A cloud!"),
    ("horse", "Why did the pony get sent to bed? Because he was a little horse!"),
    ("rabbit", "What do you call a rabbit that tells jokes? A funny bunny!"),
    ("mouse", "What do you call a mouse that can pick up a cow? Super mouse!"),
    ("lion", "What does a lion say to his friends before dinner? Let us prey!"),
    ("snake", "What do you call a snake that builds things? A boa constructor!"),
    ("spider", "What do you call a spider that loves computers? A web designer!"),
    ("penguin", "What do penguins wear on their heads? Ice caps!"),
    ("owl", "Why did the owl get invited to the party? Because he was a hoot!"),
    ("shark", "What do you call a shark that delivers letters? A postal-fish!"),
    ("whale", "Why did the whale cross the ocean? To get to the other tide!"),
    ("octopus", "Why are octopuses so good at fighting? Because they are well armed!"),
    ("crab", "Why don't crabs ever share? Because they are shellfish!"),
    ("turtle", "Why did the turtle cross the road? To get to the shell station!"),
    ("dinosaur", "What do you call a dinosaur that is sleeping? A dino snore!"),
    ("trex", "Why can't a T Rex clap? Because his arms are too short!"),
    ("dragon", "Why did the dragon stay home? He was feeling a little toasty!"),
    ("monster", "What is a monster's favorite shape? A scare-cle!"),
    ("ghost", "What do ghosts put on their cereal? Boo berries!"),
    ("skeleton", "Why didn't the skeleton go to the party? He had no body to go with!"),
    ("vampire", "What is a vampire's favorite fruit? A neck-tarine!"),
    ("witch", "How does a witch tell the time? With her witch watch!"),
    ("robot", "Why was the robot tired? He had a hard drive!"),
    ("alien", "Why did the alien go to school? To get a little brighter!"),
    ("banana", "Why did the banana go to the doctor? It wasn't peeling well!"),
    ("apple", "Why did the apple stop in the middle of the road? It ran out of juice!"),
    ("orange", "Why did the orange stop rolling? Because it ran out of juice!"),
    ("egg", "Why did the egg get thrown out of class? Because it kept cracking jokes!"),
    ("cheese", "What kind of cheese is made backwards? Edam!"),
    ("pizza", "Why did the pizza go to the doctor? It was feeling crusty!"),
    ("bread", "Why did the bread go to the doctor? It was feeling crumby!"),
    ("cookie", "Why did the cookie go to the doctor? Because it was feeling crumby!"),
    ("potato", "Why did the potato cross the road? He saw a fork up ahead!"),
    ("carrot", "What do you call a carrot that talks back? A fresh vegetable!"),
    ("corn", "Why did the corn get a prize? Because it was out-standing in its field!"),
    ("tomato", "Why did the tomato turn red? Because it saw the salad dressing!"),
    ("lemon", "Why did the lemon stop? Because it ran out of juice!"),
    ("milk", "Why did the milk go to school? To become a little smarter!"),
    ("honey", "Why do bees have sticky hair? Because they use honey combs!"),
    ("soup", "Why did the soup get a medal? Because it was souper!"),
    ("teacher", "Why did the teacher wear sunglasses? Because her class was so bright!"),
    ("school", "Why did the school book look sad? Because it had too many problems!"),
    ("book", "Why was the book so cold? It lost its jacket!"),
    ("pencil", "Why did the pencil get a prize? Because it was very sharp!"),
    ("crayon", "What do you call a crayon that is tired? A wax-ed out crayon!"),
    ("math", "Why was the math book unhappy? Because it had too many problems!"),
    ("ruler", "Why did the ruler get promoted? Because it always measured up!"),
    ("clock", "Why did the clock get in trouble? Because it kept ticking people off!"),
    ("computer", "Why was the computer cold? It left its windows open!"),
    ("moon", "Why did the Moon skip dinner? Because it was already full!"),
    ("sun", "Why did the Sun go to school? To get a little brighter!"),
    ("star", "Why did the star go to school? To become a superstar!"),
    ("rocket", "Why did the rocket go to the doctor? It needed a boost!"),
    ("planet", "Why did the planet go to the doctor? It had a ring around it!"),
    ("astronaut", "Why did the astronaut bring a broom? To sweep the stars!"),
    ("cloud", "What did one cloud say to the other? I'm feeling a little puffy!"),
    ("rain", "What did the raindrop say to the ground? I'm falling for you!"),
    ("snow", "What do you call a snowman in summer? A puddle!"),
    ("snowman", "What do snowmen eat for breakfast? Frosted flakes!"),
    ("wind", "Why did the wind go to school? To learn how to blow bubbles!"),
    ("thunder", "Why was the thunder so loud? Because it wanted to be heard!"),
    ("rainbow", "Why was the rainbow so proud? Because it really brightened the sky!"),
    ("tree", "Why was the tree so popular? Because it had a lot of friends in high places!"),
    ("flower", "What did the big flower say to the little flower? Hi, bud!"),
    ("leaf", "Why did the leaf go to the doctor? It was feeling green!"),
    ("mountain", "Why are mountains so funny? Because they are hill-arious!"),
    ("ocean", "What did the ocean say to the beach? Nothing, it just waved!"),
    ("beach", "Why is the beach always calm? Because the sea is so soothing!"),
    ("river", "Why did the river never get lost? Because it always followed its bank!"),
    ("boat", "Why did the boat go to school? To learn how to row!"),
    ("train", "Why did the train go to the doctor? It had a loco motive!"),
    ("car", "Why did the car get a flat tire? Because there was a fork in the road!"),
    ("bike", "Why couldn't the bicycle stand up? Because it was two tired!"),
    ("plane", "Why did the airplane go to the doctor? It had a bad case of the flew!"),
    ("bus", "Why did the school bus get a prize? Because it had a lot of class!"),
    ("shoe", "Why did the shoe go to the doctor? It had a sole problem!"),
    ("sock", "Why was the sock so sad? Because it lost its partner!"),
    ("hat", "Why did the hat go first? Because it was the head of the group!"),
    ("button", "Why was the button so useful? Because it always held things together!"),
    ("broom", "Why was the broom late? Because it over swept!"),
    ("chair", "Why did the chair go to the doctor? Because it was feeling a bit wobbly!"),
    ("door", "Why did the door go to school? To learn how to open up!"),
    ("window", "Why did the window go to the doctor? It had a pane in its side!"),
    ("bed", "Why did the bed go to the doctor? Because it was feeling springy!"),
    ("lamp", "Why was the lamp so happy? Because it was feeling light!"),
    ("music", "Why was the music teacher so good? Because she knew all the right notes!"),
    ("piano", "Why couldn't the piano open its door? Because its keys were inside!"),
    ("drum", "Why did the drum go to the doctor? Because it was feeling beat!"),
    ("guitar", "Why was the guitar so calm? Because it never lost its strings!"),
    ("football", "Why did the football player go to the bank? To get his quarter back!"),
    ("soccer", "Why was the soccer field so wet? Because the players dribbled all over it!"),
    ("basketball", "Why did the basketball player bring string? So he could tie the score!"),
    ("swimming", "Why is swimming so easy for fish? Because they never forget their fins!"),
    ("king", "Why did the king go to the dentist? To get his teeth crowned!"),
    ("pirate", "Why couldn't the young pirate watch the film? Because it was rated arrr!"),
    ("knight", "Why was the knight always tired? Because he worked the night shift!"),
    ("baby", "Why was the baby pencil so happy? Because it was finally getting sharp!"),
    ("doctor", "Why did the doctor carry a red pen? In case she needed to draw blood!"),
    ("farmer", "Why did the farmer win an award? Because he was outstanding in his field!"),
    ("baker", "Why did the baker work so hard? Because he kneaded the dough!"),
    ("fireman", "Why do firefighters wear red suspenders? To hold their pants up!"),
    ("police", "Why did the police officer go to the baseball game? To catch the fly ball!"),
]

# Bare "tell me a joke" has no subject, so we partition these phrasing groups
# across the first N jokes: a different way of asking gives a different joke.
BARE_GROUPS = [
    ["tell me a joke", "tell me a joke please", "can you tell me a joke"],
    ["say something funny", "be funny", "say a funny thing"],
    ["make me laugh", "can you make me laugh", "i want to laugh"],
    ["do you know any jokes", "know any jokes", "do you know a joke"],
    ["tell me another joke", "another joke", "one more joke"],
    ["tell me a funny joke", "i want a funny joke", "give me a funny joke"],
    ["tell me a silly joke", "say a silly joke", "something silly please"],
    ["do you have a joke for me", "got a joke", "have you got a joke"],
    ["cheer me up", "i am sad tell me a joke", "cheer me up please"],
    ["tell me your best joke", "what is your best joke", "your funniest joke"],
]

# Jokes the bare triggers map onto - the most universally kid-friendly ones.
BARE_PICKS = ["chicken", "cow", "skeleton", "banana", "bear",
              "snowman", "elephant", "dinosaur", "ghost", "bee"]

SUBJECT_TEMPLATES = [
    "tell me a joke about {s}",
    "tell me a {s} joke",
    "do you know a joke about {s}",
    "can you tell me a joke about {s}",
    "say a funny joke about {s}",
    "i want a joke about {s}",
    "{s} joke",
    "tell me something funny about {s}",
]

SUBJECT_EVAL_TEMPLATES = [
    "hey story tell me a joke about {s}",
    "make me laugh with a {s} joke",
]

# Spoken forms for subjects the STT would not write as one word.
SPOKEN = {"trex": "a t rex", "math": "math", "police": "a police officer",
          "fireman": "a firefighter", "soccer": "football"}


def spoken(subject: str) -> str:
    return SPOKEN.get(subject, subject)


def build():
    """Return (facts, ...) where each fact is (train_qs, eval_qs, answer)."""
    facts = []
    by_subject = dict(JOKES)
    if len(by_subject) != len(JOKES):
        raise SystemExit("duplicate joke subject - subjects are retrieval keys")

    for subject, joke in JOKES:
        s = spoken(subject)
        train = [t.format(s=s) for t in SUBJECT_TEMPLATES]
        evalq = [t.format(s=s) for t in SUBJECT_EVAL_TEMPLATES]
        facts.append((train, evalq, joke))

    for group, pick in zip(BARE_GROUPS, BARE_PICKS):
        joke = by_subject[pick]
        # First phrasing of each group is held out so bare asks are scored too.
        facts.append((group[1:], [group[0]], joke))

    return facts


def cap(s: str) -> str:
    return s[:1].upper() + s[1:] if s else s


def styled(q: str, rng: random.Random) -> str:
    """Mostly STT style (as-is); sometimes typed style with capital + '?'."""
    return q if rng.random() < 0.7 else cap(q) + "?"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="models_out/kid")
    ap.add_argument("--repeat", type=int, default=3, help="samples per training template")
    ap.add_argument("--seed", type=int, default=7)
    a = ap.parse_args()
    rng = random.Random(a.seed)

    facts = build()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    train, evals = [], []
    for tq, eq, ans in facts:
        for q in tq:
            for _ in range(a.repeat):
                train.append(f"User: {styled(q, rng)}\nBot: {ans}<|endoftext|>")
        for q in eq:
            evals.append({"q": q, "a": ans})
    rng.shuffle(train)
    (out / "joke_train.txt").write_text("\n\n".join(train) + "\n", encoding="utf-8")
    (out / "joke_eval.jsonl").write_text("\n".join(json.dumps(e) for e in evals) + "\n",
                                         encoding="utf-8")
    print(f"jokes {len(JOKES)}, facts {len(facts)}, "
          f"train samples {len(train)}, eval questions {len(evals)}")


if __name__ == "__main__":
    main()
