"""Number sense for five to eight year olds: arithmetic, multiplication,
number line, counting, adding objects, ordering numbers.

Weighted toward CONCEPTUAL explanation - what plus means, why we carry, what
times IS - because the systematic tables are generated elsewhere. Every
computable answer walks through the steps (ones, then tens, then JOIN, then
state the result), which is the thing that measured 18.3% -> 75.3% on unseen
operand pairs.
"""
from . import Fact, report


def build() -> list:
    facts = []

    def arithmetic(train, answer, evalq):
        facts.append(Fact("arithmetic", train, answer, evalq))

    def multiplication(train, answer, evalq):
        facts.append(Fact("multiplication", train, answer, evalq))

    def numberline(train, answer, evalq):
        facts.append(Fact("number line", train, answer, evalq))

    def counting(train, answer, evalq):
        facts.append(Fact("counting", train, answer, evalq))

    def objects(train, answer, evalq):
        facts.append(Fact("adding objects", train, answer, evalq))

    def ordering(train, answer, evalq):
        facts.append(Fact("ordering numbers", train, answer, evalq))

    # ------------------------------------------------------------------
    # ARITHMETIC - what the operations mean, why carrying works, worked sums
    # ------------------------------------------------------------------

    arithmetic(
        ["what does plus mean", "what is plus", "what does adding mean",
         "what does it mean to add"],
        "Plus means putting two amounts together to make a bigger amount. "
        "If you have three blocks and I give you two more you add them and get five.",
        ["what does plus do"])

    arithmetic(
        ["what does minus mean", "what does take away mean",
         "what does subtract mean", "what is subtracting"],
        "Minus means taking some away so there are fewer left. "
        "If you have six grapes and eat two you are left with four.",
        ["what does it mean to take something away"])

    arithmetic(
        ["why do we carry", "why do we carry in addition",
         "what does carrying mean", "what is carrying a one"],
        "We carry because ten ones make one ten. When the ones add up past nine "
        "you swap ten of them for one new ten and move that ten over to the tens.",
        ["why do we carry the one when we add"])

    arithmetic(
        ["what is a double", "what does double mean", "what is doubling",
         "what is a doubles fact"],
        "A double is a number added to itself. Double three is three plus three "
        "which is six. Doubles are easy to remember so they help with other sums.",
        ["what happens when you double a number"])

    arithmetic(
        ["what is a near double", "what are near doubles",
         "how do near doubles help", "what is a double plus one"],
        "A near double is almost a double. For six plus seven think double six "
        "is twelve, then add one more to get thirteen. A double you know makes it quick.",
        ["how can doubles help me add numbers that are close together"])

    arithmetic(
        ["what is double seven", "what is seven plus seven",
         "what do you get when you double seven"],
        "Seven plus seven is fourteen. Think of seven and seven as five and five "
        "making ten, plus two and two making four. Ten plus four is fourteen.",
        ["can you double seven for me"])

    arithmetic(
        ["how do i check my answer", "how can i check if my adding is right",
         "how do i know my sum is correct", "how do i check a sum"],
        "Add the numbers again the other way round. Four plus nine and nine plus "
        "four should both give thirteen. If you get the same answer twice it is right.",
        ["what is a good way to check an addition answer"])

    arithmetic(
        ["how do i check a subtraction", "how can i check my take away",
         "how do i check if i subtracted right"],
        "Add your answer back on. If ten minus four is six, then six plus four "
        "should be ten. If it comes back to where you started, the answer is right.",
        ["what is a good way to check a take away answer"])

    arithmetic(
        ["what happens when you add zero", "what is adding zero",
         "why does adding zero change nothing"],
        "Adding zero changes nothing. Zero means none, so seven plus zero is "
        "still seven. You did not put anything new into the pile.",
        ["what do you get if you add zero to a number"])

    arithmetic(
        ["what happens when you take away zero", "what is subtracting zero",
         "why does taking away zero change nothing"],
        "Taking away zero changes nothing, because you took nothing away. "
        "Nine minus zero is still nine.",
        ["what do you get if you subtract zero from a number"])

    arithmetic(
        ["what happens when you take a number away from itself",
         "what is five minus five",
         "what do you get when you subtract a number from itself"],
        "You get zero. If you have five shells and give away all five you have "
        "none left, and none is called zero.",
        ["what is left if you take away everything you have"])

    arithmetic(
        ["what is a sum", "what does sum mean",
         "what do we call the answer to an addition"],
        "A sum is the answer you get when you add. In two plus three is five, "
        "the number five is the sum.",
        ["what is the answer to an adding question called"])

    arithmetic(
        ["what is a difference in math", "what does difference mean",
         "what do we call the answer to a subtraction"],
        "The difference is the answer when you subtract. It tells you how much "
        "bigger one number is. The difference between nine and four is five.",
        ["what is the answer to a take away question called"])

    arithmetic(
        ["does it matter which order you add", "why can you add in any order",
         "is three plus five the same as five plus three"],
        "You can add in any order and get the same answer. Three plus five is "
        "eight and five plus three is eight, because the pile ends up the same.",
        ["can you swap the numbers around when you are adding"])

    arithmetic(
        ["does order matter when you subtract",
         "can you swap numbers when subtracting",
         "is nine minus two the same as two minus nine"],
        "Order does matter in subtraction. Nine minus two is seven, but two "
        "minus nine is not, because you cannot take nine things from a pile of two.",
        ["why can you not swap the numbers in a take away"])

    arithmetic(
        ["what are number bonds to ten", "how do you make ten",
         "what two numbers make ten"],
        "Pairs that make ten are one and nine, two and eight, three and seven, "
        "four and six, and five and five. Knowing them makes adding much faster.",
        ["which pairs of numbers add up to ten"])

    arithmetic(
        ["how do you add nine quickly", "what is the trick for adding nine",
         "how can i add nine in my head"],
        "Add ten and then take one back. For six plus nine, six plus ten is "
        "sixteen, and sixteen minus one is fifteen. So six plus nine is fifteen.",
        ["is there an easy way to add nine to a number"])

    arithmetic(
        ["how do you add two digit numbers", "how do you add two big numbers",
         "what are the steps for adding bigger numbers"],
        "Add the ones, then add the tens, then join the two parts. For thirty-two "
        "plus twenty-five: two plus five is seven, thirty plus twenty is fifty, "
        "fifty plus seven is fifty-seven.",
        ["what is the way to add two numbers that both have tens"])

    arithmetic(
        ["what is forty four plus forty two",
         "how do you add forty four and forty two",
         "can you work out forty four plus forty two"],
        "Four plus two is six. Forty plus forty is eighty. Eighty plus six is "
        "eighty-six. So forty-four plus forty-two is eighty-six.",
        ["forty four add forty two"])

    arithmetic(
        ["what is twenty seven plus fifteen",
         "how do you add twenty seven and fifteen",
         "can you work out twenty seven plus fifteen"],
        "Seven plus five is twelve. Twenty plus ten is thirty. Thirty plus twelve "
        "is forty-two. So twenty-seven plus fifteen is forty-two.",
        ["twenty seven add fifteen"])

    arithmetic(
        ["what is thirty six plus twenty eight",
         "how do you add thirty six and twenty eight",
         "can you work out thirty six plus twenty eight"],
        "Six plus eight is fourteen. Thirty plus twenty is fifty. Fifty plus "
        "fourteen is sixty-four. So thirty-six plus twenty-eight is sixty-four.",
        ["thirty six add twenty eight"])

    arithmetic(
        ["what is sixty one plus twenty seven",
         "how do you add sixty one and twenty seven",
         "can you work out sixty one plus twenty seven"],
        "One plus seven is eight. Sixty plus twenty is eighty. Eighty plus eight "
        "is eighty-eight. So sixty-one plus twenty-seven is eighty-eight.",
        ["sixty one add twenty seven"])

    arithmetic(
        ["what is nineteen plus nineteen", "how do you add nineteen and nineteen",
         "can you work out nineteen plus nineteen"],
        "Nine plus nine is eighteen. Ten plus ten is twenty. Twenty plus eighteen "
        "is thirty-eight. So nineteen plus nineteen is thirty-eight.",
        ["nineteen add nineteen"])

    arithmetic(
        ["what is borrowing in subtraction", "what does regrouping mean",
         "why do we borrow a ten"],
        "When the ones are too small to take from, you break one ten into ten "
        "ones and move them over. Then there are enough ones to take away.",
        ["why do we break a ten when we subtract"])

    arithmetic(
        ["what is fifty three minus twenty one",
         "how do you subtract twenty one from fifty three",
         "can you work out fifty three minus twenty one"],
        "Take the tens first. Fifty minus twenty is thirty. Three minus one is "
        "two. Thirty plus two is thirty-two. So fifty-three minus twenty-one is "
        "thirty-two.",
        ["fifty three take away twenty one"])

    arithmetic(
        ["what is fifty two minus twenty seven",
         "how do you subtract twenty seven from fifty two",
         "can you work out fifty two minus twenty seven"],
        "Fifty-two minus twenty is thirty-two. Now take away seven more. "
        "Thirty-two minus two is thirty, and thirty minus five is twenty-five. "
        "So fifty-two minus twenty-seven is twenty-five.",
        ["fifty two take away twenty seven"])

    arithmetic(
        ["what is forty five minus eighteen",
         "how do you subtract eighteen from forty five",
         "can you work out forty five minus eighteen"],
        "Forty-five minus ten is thirty-five. Now take away eight more. "
        "Thirty-five minus five is thirty, and thirty minus three is twenty-seven. "
        "So forty-five minus eighteen is twenty-seven.",
        ["forty five take away eighteen"])

    arithmetic(
        ["at the shop i buy five apples and three oranges how many pieces of fruit",
         "if you buy five apples and three oranges how much fruit is that",
         "how many fruits are five apples and three oranges"],
        "Put the two groups together. Five plus three is eight. So you have "
        "eight pieces of fruit.",
        ["i have five apples and three oranges how many fruits do i have"])

    arithmetic(
        ["i have seven toy cars and get four more how many do i have",
         "if you have seven cars and get four more how many cars",
         "what is seven toy cars plus four toy cars"],
        "Count on from seven. Seven plus three is ten, and one more is eleven. "
        "So you have eleven toy cars.",
        ["i had seven cars and then got four more how many now"])

    arithmetic(
        ["i have twelve crackers and eat five how many are left",
         "if you eat five of your twelve crackers how many are left",
         "what is twelve crackers take away five"],
        "Take five away from twelve. Twelve minus two is ten, and ten minus "
        "three is seven. So seven crackers are left.",
        ["there were twelve crackers and i ate five how many now"])

    arithmetic(
        ["a sticker costs six cents and a pencil costs seven cents how much is that",
         "how much are a six cent sticker and a seven cent pencil",
         "what do a six cent sticker and a seven cent pencil cost together"],
        "Add the two prices. Six plus six is twelve, and one more is thirteen. "
        "So they cost thirteen cents together.",
        ["if i buy a six cent sticker and a seven cent pencil what do i pay"])

    arithmetic(
        ["there are eight kids at the party and six more arrive how many kids",
         "eight children were playing and six more came how many are there",
         "what is eight kids plus six kids"],
        "Eight plus two is ten, and there are four more to add. Ten plus four is "
        "fourteen. So there are fourteen children at the party.",
        ["eight friends were there and six more showed up how many now"])

    arithmetic(
        ["how do you add three numbers", "can you add three numbers at once",
         "what do you do when there are three numbers to add"],
        "Add two of them first, then add the third to that answer. For two plus "
        "three plus four, two plus three is five, and five plus four is nine.",
        ["how do i add up three numbers together"])

    arithmetic(
        ["what does equals mean", "what is the equals sign",
         "what does the equals sign tell you"],
        "Equals means both sides are worth the same amount. In four plus one "
        "equals five, the four and the one together are worth exactly five.",
        ["what does it mean when you say equals"])

    arithmetic(
        ["what is a number sentence", "what does a number sentence mean",
         "what is a math sentence"],
        "A number sentence is a little math story written with numbers and signs, "
        "like three plus two equals five. It says what happened and what the answer is.",
        ["what do we call something like two plus two equals four"])

    arithmetic(
        ["what is a fact family", "what does fact family mean",
         "how are adding and subtracting connected"],
        "Adding and subtracting undo each other. From two, three and five you get "
        "two plus three is five, and five minus three is two, and five minus two is three.",
        ["how do adding and taking away go together"])

    arithmetic(
        ["what happens when you add ten to a number", "how do you add ten",
         "why is adding ten easy"],
        "Adding ten only changes the tens. Twenty-three plus ten is thirty-three, "
        "and forty-six plus ten is fifty-six. The ones stay exactly the same.",
        ["what is the quick way to add ten to a number"])

    arithmetic(
        ["what happens when you take ten away", "how do you subtract ten",
         "why is taking away ten easy"],
        "Taking ten away drops the tens by one and leaves the ones alone. "
        "Fifty-seven minus ten is forty-seven.",
        ["what is the quick way to subtract ten from a number"])

    arithmetic(
        ["what does counting on mean", "how do you count on to add",
         "how do i add by counting on"],
        "Start at the bigger number and count the smaller one forward. For eight "
        "plus three say eight, then nine, ten, eleven. So eight plus three is eleven.",
        ["how can counting forward help me add"])

    arithmetic(
        ["how do you count back to subtract", "what does counting back mean",
         "how do i take away by counting back"],
        "Start at the first number and count backward. For ten minus three say "
        "ten, then nine, eight, seven. So ten minus three is seven.",
        ["how can counting backward help me subtract"])

    arithmetic(
        ["why do we learn adding", "why is adding useful",
         "when would i need to add"],
        "Adding tells you how many you end up with when things come together. "
        "You add to count your toys, share out snacks, or find out if you have "
        "enough money.",
        ["what is adding good for in real life"])

    # ------------------------------------------------------------------
    # MULTIPLICATION - what times IS, arrays, order, skip counting, squares
    # ------------------------------------------------------------------

    multiplication(
        ["what does times mean", "what is multiplication", "what does multiply mean",
         "what does times do"],
        "Times means adding the same number over and over. Three times four means "
        "four counted three times, which is four plus four plus four.",
        ["what is multiplying"])

    multiplication(
        ["is multiplication just adding", "how is multiplying like adding",
         "why is times the same as repeated adding"],
        "Yes. Multiplying is a fast way to add equal groups. Instead of saying "
        "five plus five plus five you say three times five, and both give fifteen.",
        ["can i do a multiplication by adding"])

    multiplication(
        ["what does groups of mean", "what is a group in multiplication",
         "what does four groups of two mean"],
        "Groups of means equal piles. Four groups of two is four piles with two "
        "in each. Two plus two plus two plus two is eight, so four times two is eight.",
        ["what do we mean by equal groups in a times question"])

    multiplication(
        ["what is an array", "what does array mean in math",
         "how does an array help with multiplication"],
        "An array is dots set out in neat rows and columns. Three rows of five "
        "dots show three times five. You can count the dots to find the answer.",
        ["what do we call dots set out in rows and columns"])

    multiplication(
        ["does it matter which way round you multiply",
         "is three times four the same as four times three",
         "why can you swap the numbers in multiplication"],
        "You can swap them and get the same answer. Three rows of four dots and "
        "four rows of three dots are the same array turned sideways, and both hold twelve.",
        ["can you turn a times question around"])

    multiplication(
        ["what is three times four", "how do you work out three times four",
         "what is four added three times"],
        "Three times four means four plus four plus four. Four plus four is "
        "eight. Eight plus four is twelve. So three times four is twelve.",
        ["three times four please"])

    multiplication(
        ["what is five times six", "how do you work out five times six",
         "what is six added five times"],
        "Five times six means five groups of six. Skip count by sixes: six, "
        "twelve, eighteen, twenty-four, thirty. So five times six is thirty.",
        ["five times six please"])

    multiplication(
        ["what is seven times eight", "how do you work out seven times eight",
         "what is eight added seven times"],
        "Seven times eight means seven groups of eight. Five eights are forty. "
        "Two more eights are sixteen. Forty plus sixteen is fifty-six. So seven "
        "times eight is fifty-six.",
        ["seven times eight please"])

    multiplication(
        ["what is six times six", "how do you work out six times six",
         "what is six added six times"],
        "Six times six means six groups of six. Skip count by sixes: six, twelve, "
        "eighteen, twenty-four, thirty, thirty-six. So six times six is thirty-six.",
        ["six times six please"])

    multiplication(
        ["what is four times five", "how do you work out four times five",
         "what is five added four times"],
        "Four times five means four groups of five. Skip count by fives: five, "
        "ten, fifteen, twenty. So four times five is twenty.",
        ["four times five please"])

    multiplication(
        ["what is nine times three", "how do you work out nine times three",
         "what is three added nine times"],
        "Nine times three is the same as three times nine, which is easier. "
        "Three groups of nine: nine, eighteen, twenty-seven. So nine times three "
        "is twenty-seven.",
        ["nine times three please"])

    multiplication(
        ["what is skip counting", "what does skip counting mean",
         "how does skip counting help with times"],
        "Skip counting is counting in equal jumps. Counting by threes goes three, "
        "six, nine, twelve. Each jump adds one more group, so it lands on the "
        "times answers.",
        ["what does it mean to count in jumps"])

    multiplication(
        ["what is a square number", "what does square number mean",
         "why are they called square numbers"],
        "A square number comes from multiplying a number by itself. Four times "
        "four is sixteen, and sixteen dots fit into a perfect square of four rows of four.",
        ["what makes a number a square number"])

    multiplication(
        ["what are the first square numbers", "can you name some square numbers",
         "which numbers are square numbers"],
        "One, four, nine, sixteen, twenty-five and thirty-six are square numbers. "
        "They come from one times one, two times two, three times three and so on.",
        ["tell me a few square numbers"])

    multiplication(
        ["what happens when you times by zero", "what is a number times zero",
         "why is times zero always zero"],
        "Anything times zero is zero. Six times zero means six empty groups, and "
        "six empty groups still hold nothing at all.",
        ["what do you get when you multiply by zero"])

    multiplication(
        ["what happens when you times by one", "what is a number times one",
         "why does times one stay the same"],
        "Any number times one stays the same. Nine times one is one group of "
        "nine, which is just nine.",
        ["what do you get when you multiply by one"])

    multiplication(
        ["what happens when you times by ten", "how do you multiply by ten",
         "why is times ten easy"],
        "Times ten puts a zero on the end. Four times ten is forty and seven "
        "times ten is seventy, because every group is a whole ten.",
        ["what is the trick for multiplying by ten"])

    multiplication(
        ["what does times two mean", "is times two the same as doubling",
         "why is multiplying by two easy"],
        "Times two is the same as doubling. Seven times two is seven plus seven, "
        "which is fourteen.",
        ["what happens when you multiply a number by two"])

    multiplication(
        ["what is the pattern for the five times table", "how do you count by fives in times",
         "why do the fives end in zero or five"],
        "Counting by fives goes five, ten, fifteen, twenty, twenty-five. The "
        "answers always end in five or zero, so they are easy to spot.",
        ["what pattern do the five times answers make"])

    multiplication(
        ["what is a product in math", "what does product mean",
         "what do we call the answer to a times question"],
        "The product is the answer when you multiply. In three times four is "
        "twelve, the number twelve is the product.",
        ["what is the answer to a multiplication called"])

    multiplication(
        ["what is a factor", "what does factor mean",
         "what are the numbers you multiply together called"],
        "Factors are the numbers you multiply together. In two times five is ten, "
        "the factors are two and five, and the answer ten is the product.",
        ["what do we call the two numbers in a times question"])

    multiplication(
        ["why do we use multiplication", "when would i need to multiply",
         "what is multiplication good for"],
        "Multiplying saves time when things come in equal groups. If six boxes "
        "each hold four crayons, six times four tells you there are twenty-four crayons.",
        ["why is multiplying helpful"])

    multiplication(
        ["what if i forget a times fact",
         "how can i work out a times answer i do not know",
         "what do i do if i cannot remember a times table"],
        "Build it from one you know. If you forget six times seven, five sevens "
        "are thirty-five and one more seven is seven. Thirty-five plus seven is forty-two.",
        ["how do i figure out a multiplication i have forgotten"])

    multiplication(
        ["how is multiplication different from addition",
         "what is the difference between plus and times",
         "why is times not the same as plus"],
        "Plus joins two amounts once. Times joins the same amount again and "
        "again. Three plus four is seven, but three times four is twelve.",
        ["how are plus and times different"])

    # ------------------------------------------------------------------
    # NUMBER LINE
    # ------------------------------------------------------------------

    numberline(
        ["what is a number line", "what does a number line look like",
         "what is a number line for"],
        "A number line is a straight line with the numbers marked in order. Small "
        "numbers sit on the left and bigger ones on the right, so you can see how "
        "they fit together.",
        ["can you tell me about number lines"])

    numberline(
        ["what does before mean in numbers", "what number comes before",
         "how do i find the number before"],
        "The number before is one less, and it sits just to the left on the "
        "number line. The number before seven is six.",
        ["how do you know which number comes just before another"])

    numberline(
        ["what number comes after", "what does after mean in numbers",
         "how do i find the number after"],
        "The number after is one more, and it sits just to the right on the "
        "number line. The number after seven is eight.",
        ["how do you know which number comes just after another"])

    numberline(
        ["what does between mean in numbers", "which number is between two numbers",
         "how do i find the number in the middle"],
        "A number is between two others if it comes after the first and before "
        "the second. Six is between five and seven.",
        ["what does it mean for a number to sit in between two others"])

    numberline(
        ["what comes before twenty", "what number is just before twenty",
         "which number comes right before twenty"],
        "Nineteen comes before twenty, because nineteen plus one is twenty.",
        ["what is one less than twenty"])

    numberline(
        ["what comes after twenty nine", "what number is just after twenty nine",
         "which number comes right after twenty nine"],
        "Thirty comes after twenty-nine. The nine ones fill up into a whole ten, "
        "so twenty-nine plus one is thirty.",
        ["what is one more than twenty nine"])

    numberline(
        ["which number is further along the number line",
         "how does a number line show which is bigger",
         "how do i tell which number is bigger on a number line"],
        "The one further to the right is bigger. Eight sits to the right of "
        "three, so eight is bigger than three.",
        ["how can a number line show me the bigger number"])

    numberline(
        ["what is the distance between two numbers", "how far apart are two numbers",
         "what does distance mean on a number line"],
        "Count the hops from one number to the other, or take the smaller one "
        "from the bigger one. From three to nine is six hops, so they are six apart.",
        ["how do i find out how far apart two numbers are"])

    numberline(
        ["how far apart are four and nine", "what is the distance from four to nine",
         "how many hops from four to nine"],
        "Count the hops from four to nine: five, six, seven, eight, nine. That is "
        "five hops. So four and nine are five apart.",
        ["what is the gap between four and nine"])

    numberline(
        ["how far apart are twelve and twenty",
         "what is the distance from twelve to twenty",
         "how many hops from twelve to twenty"],
        "Take twelve away from twenty. Twenty minus ten is ten, and ten minus two "
        "is eight. So twelve and twenty are eight apart.",
        ["what is the gap between twelve and twenty"])

    numberline(
        ["how do you add on a number line", "how do i use a number line to add",
         "what do you do to add with a number line"],
        "Put your finger on the first number and hop forward as many steps as the "
        "second number says. Wherever you land is the answer.",
        ["how does a number line help me add"])

    numberline(
        ["how do you do seven plus five on a number line",
         "show me seven plus five with hops",
         "what happens if you start at seven and hop five"],
        "Start at seven. Hop forward five ones: eight, nine, ten, eleven, twelve. "
        "You land on twelve. So seven plus five is twelve.",
        ["use a number line to work out seven plus five"])

    numberline(
        ["how do you subtract on a number line",
         "how do i use a number line to take away",
         "which way do you hop to subtract"],
        "Hop backward, to the left. Start at eleven and hop back four: ten, nine, "
        "eight, seven. So eleven minus four is seven.",
        ["how does a number line help me take away"])

    numberline(
        ["where is zero on the number line", "what is at the start of the number line",
         "why does the number line start at zero"],
        "Zero sits at the start, to the left of one. It means none, so every "
        "counting number is somewhere to the right of it.",
        ["where does the number line begin"])

    numberline(
        ["how do you jump by tens on a number line",
         "what does a jump of ten look like",
         "how do i add ten on a number line"],
        "One big jump of ten is quicker than ten little hops. From twenty-four a "
        "jump of ten lands on thirty-four, and another lands on forty-four.",
        ["how do i make big jumps along a number line"])

    numberline(
        ["how does a number line help with ordering",
         "how do i put numbers in order using a number line",
         "can a number line show me the order"],
        "Find each number on the line, then read them off from left to right. "
        "That puts them in order from smallest to biggest.",
        ["can a number line help me put numbers in order"])

    numberline(
        ["which number is halfway between zero and ten",
         "what is in the middle of zero and ten", "what number is halfway to ten"],
        "Five is halfway. It is five hops from zero and five more hops on to ten.",
        ["what sits right in the middle between zero and ten"])

    numberline(
        ["which numbers are between three and seven",
         "what comes between three and seven", "name the numbers between three and seven"],
        "Four, five and six are between three and seven.",
        ["what numbers sit in between three and seven"])

    numberline(
        ["how do i tell which number is closer", "which number is nearer on the number line",
         "how do i know which is closest"],
        "Count the hops to each one. From eight to ten is two hops, but from "
        "eight back to five is three hops, so ten is closer to eight.",
        ["how do you work out which number is nearer"])

    numberline(
        ["what does one more mean", "what does one less mean",
         "what is one more and one less"],
        "One more means the next number to the right, and one less means the one "
        "to the left. One more than nine is ten, and one less than nine is eight.",
        ["what do one more and one less mean on a number line"])

    # ------------------------------------------------------------------
    # COUNTING
    # ------------------------------------------------------------------

    counting(
        ["count on four from seven", "what do you get if you count on four from seven",
         "start at seven and count on four"],
        "Start at seven and say eight, nine, ten, eleven. You counted four more, "
        "so you land on eleven.",
        ["begin at seven and count four more where do you reach"])

    counting(
        ["count back three from twelve",
         "what do you get if you count back three from twelve",
         "start at twelve and count back three"],
        "Start at twelve and say eleven, ten, nine. You counted back three, so "
        "you land on nine.",
        ["begin at twelve and count three backward where do you stop"])

    counting(
        ["how do you count by twos", "what is counting in twos", "can you count by twos"],
        "Counting by twos goes two, four, six, eight, ten, twelve, fourteen, "
        "sixteen, eighteen, twenty. You skip every other number.",
        ["say the numbers when you count in twos"])

    counting(
        ["how do you count by fives", "what is counting in fives", "can you count by fives"],
        "Counting by fives goes five, ten, fifteen, twenty, twenty-five, thirty, "
        "thirty-five, forty, forty-five, fifty.",
        ["say the numbers when you count in fives"])

    counting(
        ["how do you count by tens", "what is counting in tens", "can you count by tens"],
        "Counting by tens goes ten, twenty, thirty, forty, fifty, sixty, seventy, "
        "eighty, ninety, one hundred.",
        ["say the numbers when you count in tens"])

    counting(
        ["why do we count by twos", "why is skip counting useful",
         "why not count everything one at a time"],
        "Skip counting is faster and you make fewer mistakes. Counting socks in "
        "twos means ten pairs is twenty socks in only ten words.",
        ["what is the point of counting in jumps instead of ones"])

    counting(
        ["what does estimate mean", "what is estimating",
         "what does it mean to guess how many"],
        "Estimating is a smart guess about how many, without counting them all. "
        "You look, think about a group you already know, and say a close number.",
        ["what do we call a good guess about how many"])

    counting(
        ["how do i estimate how many", "how do you make a good estimate",
         "what is a good way to estimate"],
        "Count a small part, then imagine how many parts there are. If ten "
        "candies fill one corner of the jar and there are about five corners, "
        "guess about fifty.",
        ["how can i guess how many there are without counting"])

    counting(
        ["what is one to one counting", "why do you touch each thing when counting",
         "what does one to one correspondence mean"],
        "Say exactly one number for each thing and touch it as you go. That way "
        "nothing gets counted twice and nothing gets missed.",
        ["why should i point at each thing as i count it"])

    counting(
        ["how do i know how many there are after counting",
         "what does the last number you say mean",
         "why does the last number tell you how many"],
        "The last number you say is how many there are altogether. If you finish "
        "on nine, then there are nine things.",
        ["what does the final number in a count tell me"])

    counting(
        ["what does zero mean", "what is zero", "what does none mean in numbers"],
        "Zero means none at all. If a plate has no cookies left on it, there are "
        "zero cookies.",
        ["which number means nothing at all"])

    counting(
        ["what does first mean", "what are ordinal numbers",
         "what do first and second mean"],
        "Ordinal numbers tell you the position in a line. First, second, third, "
        "fourth and fifth say who is where, not how many there are.",
        ["what do we call words like first and second"])

    counting(
        ["what comes after third", "what are the first few ordinal numbers",
         "can you say the order words"],
        "First, second, third, fourth, fifth, sixth, seventh, eighth, ninth, "
        "tenth. Fourth is the one that comes after third.",
        ["what position comes after first and second and third"])

    counting(
        ["what is the difference between three and third",
         "why do we say third instead of three", "what does third mean"],
        "Three tells how many and third tells which place in line. Three children "
        "means a group of three, but the third child is the one after the second.",
        ["how is third different from three"])

    counting(
        ["what comes after ninety nine", "how high can you count",
         "what is the biggest two digit number"],
        "One hundred comes after ninety-nine. Ninety-nine is the biggest two "
        "digit number, and one more turns it into one hundred.",
        ["which number comes right after ninety nine"])

    counting(
        ["can you count backward from twenty", "how do you count down from twenty",
         "count down from twenty"],
        "Twenty, nineteen, eighteen, seventeen, sixteen, fifteen, fourteen, "
        "thirteen, twelve, eleven, ten, and on down to zero.",
        ["say the numbers going down from twenty"])

    counting(
        ["what is an even number", "what makes a number even", "which numbers are even"],
        "Even numbers share into two equal groups with none left over. They are "
        "the counting by twos numbers: two, four, six, eight and ten.",
        ["how do i know if a number is even"])

    counting(
        ["what is an odd number", "what makes a number odd", "which numbers are odd"],
        "Odd numbers always have one left over when you share them between two. "
        "One, three, five, seven and nine are odd.",
        ["how do i know if a number is odd"])

    counting(
        ["how many tens are in thirty", "how many tens make thirty",
         "what does thirty mean in tens"],
        "Thirty is three tens. Ten plus ten is twenty, and twenty plus ten is "
        "thirty.",
        ["how many tens does thirty have in it"])

    counting(
        ["what do the tens and ones mean",
         "what does the first digit in a two digit number mean",
         "what does forty seven mean"],
        "The first digit counts tens and the second counts ones. Forty-seven is "
        "four tens and seven ones, which is forty plus seven.",
        ["what do the two parts of a two digit number stand for"])

    counting(
        ["how do i count a big pile", "what is the best way to count lots of things",
         "how can i count many things without losing track"],
        "Move each thing to a new pile as you count it, or make groups of ten. "
        "Then count the tens in jumps and add on the leftovers.",
        ["how do you count a lot of objects carefully"])

    counting(
        ["can you count by twos starting at one", "what if you count in twos from one",
         "how do you skip count in twos from one"],
        "Starting at one you get one, three, five, seven, nine and eleven. Those "
        "are the odd numbers.",
        ["what numbers do you get counting in twos from one"])

    counting(
        ["if i am third in line how many people are in front of me",
         "how many are ahead of the third person", "what does being third in line mean"],
        "Two people are in front of you. The first and the second are ahead, and "
        "then it is your turn.",
        ["i am third in the line who is before me"])

    # "how many fingers on two hands" moved to games_play.py, which owns the
    # counting-on-hands topic. This one keeps the general question because it
    # is the only version that shows the work.
    counting(
        ["how many fingers do i have", "how do i count my fingers",
         "how do i work out how many fingers i have"],
        "Five on one hand and five on the other. Five plus five is ten, so you "
        "have ten fingers.",
        ["how many fingers are there on both hands"])

    counting(
        ["why do we count", "what is counting for", "why is counting useful"],
        "Counting tells you how many. You count to share snacks fairly, to check "
        "nothing is missing, and to know when it is your turn.",
        ["what is counting good for"])

    # ------------------------------------------------------------------
    # ADDING OBJECTS - grouping, sharing, leftovers, comparing
    # ------------------------------------------------------------------

    objects(
        ["what does grouping mean", "why do we group things",
         "what is a group of objects"],
        "Grouping means putting things into equal piles. Groups are easier to "
        "count, and ten marbles in two piles of five are still ten marbles.",
        ["why is it helpful to put things into groups"])

    objects(
        ["what does sharing equally mean", "what is fair sharing",
         "what does it mean to share the same amount"],
        "Sharing equally means everybody gets the same amount. You hand them out "
        "one at a time, going round and round, until they are all gone.",
        ["how do you share something out fairly"])

    objects(
        ["how do you share twelve cookies between three children",
         "what is twelve shared by three",
         "if three children share twelve cookies how many each"],
        "Deal them out one at a time. Each child gets one, then two, then three, "
        "then four. Three groups of four is twelve, so each child gets four cookies.",
        ["twelve cookies for three kids how many each"])

    objects(
        ["how do you share ten cookies between two people",
         "what is ten shared by two",
         "if two friends share ten cookies how many each"],
        "Deal them out, one for you and one for me. Five plus five is ten, so "
        "each person gets five cookies.",
        ["ten cookies split between two children how many each"])

    objects(
        ["what happens if seven candies are shared between two children",
         "what is seven shared by two", "how do you share seven between two"],
        "Give one to each in turn. Each child gets three and one is left over. "
        "Three plus three is six, and six plus one is seven.",
        ["seven candies for two kids what happens"])

    objects(
        ["what does left over mean", "what is a remainder",
         "what happens if it does not share evenly"],
        "The left over is what will not fit into the equal groups. Sharing seven "
        "between two gives three each, with one left over.",
        ["what do you call the bit left after sharing"])

    objects(
        ["how do i tell which group has more", "how do you compare two groups",
         "what does more mean"],
        "Line them up side by side and match them one to one. Whichever group "
        "still has some left when the other runs out is the one with more.",
        ["how can i see which pile is bigger"])

    objects(
        ["what does how many more mean", "how do i find how many more one group has",
         "how do you work out the difference between two groups"],
        "Take the smaller number away from the bigger one. If you have nine "
        "stickers and I have four, nine minus four is five, so you have five more.",
        ["how do i find out how many extra one group has"])

    objects(
        ["i have eight blocks and you have five how many more do i have",
         "what is the difference between eight blocks and five blocks",
         "how many more is eight than five"],
        "Match them one to one. Five pair up with five, and three blocks are left "
        "with nothing to match. So eight is three more than five.",
        ["eight blocks against five blocks how many extra"])

    objects(
        ["what does fewer mean", "what does less mean with objects",
         "how do i say which group has less"],
        "Fewer means not as many. If you have three grapes and I have seven, you "
        "have fewer grapes than me.",
        ["which word means not as many"])

    objects(
        ["what happens when you put two groups together",
         "how do you find the total of two groups", "what does total mean"],
        "You add them. A group of four and a group of six joined together make "
        "four plus six, which is ten. That answer is called the total.",
        ["how many do you have when two groups join up"])

    objects(
        ["what are equal groups", "how do i know if groups are equal",
         "what does equal groups mean"],
        "Groups are equal when each one holds exactly the same number. Three "
        "plates with two cookies on each are equal groups.",
        ["when are two piles exactly the same size"])

    objects(
        ["how is sharing like multiplying", "what is the opposite of multiplying",
         "how are groups and times connected"],
        "Sharing undoes multiplying. Three groups of four make twelve, so twelve "
        "shared between three gives four each.",
        ["how does sharing go together with times"])

    objects(
        ["how do you share six apples between three children",
         "what is six shared by three",
         "if three children share six apples how many each"],
        "Give one to each child, then go round again. Each child gets two, and "
        "three groups of two is six.",
        ["six apples for three kids how many each"])

    objects(
        ["what happens if nine candies are shared between four children",
         "how do you share nine between four", "what is nine shared by four"],
        "Each child gets two and one is left over. Four groups of two is eight, "
        "and eight plus one is nine.",
        ["nine candies for four children what happens"])

    objects(
        ["how do i find the biggest group", "which group has the most",
         "how do you compare three piles"],
        "Count each pile and remember the numbers, then compare the numbers. The "
        "biggest number is the biggest pile.",
        ["how do i work out which pile has the most in it"])

    objects(
        ["how do i know if two groups have the same",
         "what does the same amount mean", "when do two groups match"],
        "Match them one for one. If they both run out at the same moment the "
        "groups are equal, and we say they have the same amount.",
        ["how can i tell two piles hold the same number"])

    objects(
        ["why do we group things in tens", "what is a group of ten good for",
         "how does making tens help counting"],
        "Tens are easy to count in jumps. Four piles of ten and three spare is "
        "forty plus three, which is forty-three.",
        ["why make piles of ten when you are counting"])

    objects(
        ["what does one for you one for me mean", "how do you deal things out",
         "what is dealing out"],
        "You give one to each person in turn, round and round, until they are all "
        "gone. That way nobody ends up with more than anybody else.",
        ["what is the way to hand things out one at a time"])

    objects(
        ["i have four red blocks and seven blue blocks how many blocks",
         "how many blocks are four red and seven blue",
         "what is four red blocks plus seven blue blocks"],
        "Start at seven and count on four: eight, nine, ten, eleven. So you have "
        "eleven blocks altogether.",
        ["four red and seven blue blocks how many altogether"])

    objects(
        ["can everything be shared equally", "why can some things not be shared evenly",
         "what if the sharing does not work out"],
        "Not always. If there are five buttons and two children, each gets two "
        "and one is stuck in the middle. That spare one is the left over.",
        ["is it always possible to share things out evenly"])

    # ------------------------------------------------------------------
    # ORDERING NUMBERS
    # ------------------------------------------------------------------

    ordering(
        ["what does bigger mean in numbers", "how do i know which number is bigger",
         "what does greater mean"],
        "The bigger number is the one you reach later when you count. You say "
        "eight after five, so eight is bigger than five.",
        ["how do you tell which number is greater"])

    ordering(
        ["what does smaller mean in numbers", "how do i know which number is smaller",
         "what does less than mean"],
        "The smaller number comes first when you count. Three comes before six, "
        "so three is smaller than six.",
        ["how do you tell which number is less"])

    ordering(
        ["what does equal mean in numbers", "when are two numbers equal",
         "what does the same number mean"],
        "Two numbers are equal when they are worth exactly the same. Six and six "
        "are equal, and so are five plus one and six.",
        ["when do two numbers match exactly"])

    ordering(
        ["name a number between four and nine",
         "which numbers come between four and nine", "what is in between four and nine"],
        "Five, six, seven and eight all come between four and nine. Each one is "
        "bigger than four and smaller than nine.",
        ["tell me a number that sits between four and nine"])

    ordering(
        ["what does ascending mean", "what is ascending order",
         "what does smallest to biggest mean"],
        "Ascending order means going up from smallest to biggest, like three, "
        "five, eight, eleven. Think of climbing up the stairs.",
        ["what do you call order from small to big"])

    ordering(
        ["what does descending mean", "what is descending order",
         "what does biggest to smallest mean"],
        "Descending order means going down from biggest to smallest, like eleven, "
        "eight, five, three. Think of walking down the stairs.",
        ["what do you call order from big to small"])

    ordering(
        ["how do you compare two digit numbers",
         "how do i tell which two digit number is bigger",
         "what do you look at first when comparing bigger numbers"],
        "Look at the tens first. The number with more tens is bigger. Only if the "
        "tens are the same do you need to compare the ones.",
        ["what is the rule for comparing two digit numbers"])

    ordering(
        ["which is bigger forty seven or fifty two",
         "is fifty two more than forty seven", "compare forty seven and fifty two"],
        "Look at the tens first. Forty is less than fifty, so forty-seven is "
        "smaller than fifty-two. When the tens differ you do not need the ones.",
        ["which number is greater forty seven or fifty two"])

    ordering(
        ["which is bigger sixty three or sixty eight",
         "is sixty eight more than sixty three", "compare sixty three and sixty eight"],
        "The tens are the same, sixty and sixty, so look at the ones. Three is "
        "less than eight. So sixty-three is smaller than sixty-eight.",
        ["which number is greater sixty three or sixty eight"])

    ordering(
        ["which is bigger eighty one or seventy nine",
         "is eighty one more than seventy nine", "compare eighty one and seventy nine"],
        "Compare the tens. Eighty is more than seventy, so eighty-one is bigger "
        "than seventy-nine, even though that nine looks large.",
        ["which number is greater eighty one or seventy nine"])

    ordering(
        ["which is bigger twenty six or sixty two",
         "is sixty two more than twenty six", "compare twenty six and sixty two"],
        "Same digits, different places. Twenty-six has two tens and sixty-two has "
        "six tens. Six tens beat two tens, so sixty-two is bigger.",
        ["which number is greater twenty six or sixty two"])

    ordering(
        ["how do i put three numbers in order", "how do you order numbers",
         "what is the way to sort numbers"],
        "Find the smallest and write it first, then the smallest of what is left, "
        "and so on. For nine, four and six you get four, six, nine.",
        ["how do i sort numbers from smallest to biggest"])

    ordering(
        ["put twenty three fifteen and thirty one in order",
         "which order do twenty three fifteen and thirty one go in",
         "sort fifteen thirty one and twenty three"],
        "Compare the tens: ten, twenty and thirty. So from smallest to biggest "
        "they go fifteen, twenty-three, thirty-one.",
        ["order these numbers fifteen twenty three and thirty one"])

    ordering(
        ["how do i find the biggest number", "which number is the largest",
         "how do you pick out the biggest"],
        "Compare them two at a time and keep the winner each time. The number "
        "that beats all the others is the biggest.",
        ["how do i spot the largest number in a list"])

    ordering(
        ["how do i find the smallest number", "which number is the least",
         "how do you pick out the smallest"],
        "Compare them two at a time and keep the smaller one each time. The "
        "number left standing at the end is the smallest.",
        ["how do i spot the smallest number in a list"])

    ordering(
        ["what is the greater than sign", "what does the pointy sign mean",
         "how do you write bigger than"],
        "The greater than sign opens toward the bigger number, like a hungry "
        "mouth turning to eat the larger amount. It says the first number is bigger.",
        ["what does that arrow shape between two numbers mean"])

    ordering(
        ["what if two numbers have the same tens",
         "how do i compare numbers with the same tens",
         "what do you do when the tens match"],
        "When the tens match, the ones decide. Thirty-four and thirty-nine both "
        "have three tens, but nine is more than four, so thirty-nine is bigger.",
        ["what happens when both numbers have the same tens digit"])

    ordering(
        ["is a two digit number always bigger than a one digit number",
         "which is bigger nine or twelve", "is twelve bigger than nine"],
        "A two digit number is always bigger. Twelve has one ten and two ones, "
        "while nine has no tens at all, so twelve is bigger than nine.",
        ["can a one digit number be bigger than a two digit one"])

    ordering(
        ["how does counting help me order numbers",
         "why does counting order tell you which is bigger",
         "how do i use counting to compare"],
        "Say the counting numbers in order. Whichever number you reach later is "
        "the bigger one, because you had to count further to get there.",
        ["can counting tell me which number is bigger"])

    ordering(
        ["is zero the smallest number", "what is the smallest counting number",
         "which number is the least of all"],
        "Zero is the smallest when you are counting things, because it means "
        "none. Every other counting number is bigger than zero.",
        ["what is the very smallest number when you count"])

    ordering(
        ["why do we put numbers in order", "why is ordering numbers useful",
         "when would i need to order numbers"],
        "Ordering helps you see who has the most, find your place in a line, or "
        "put things in the right spot. It turns a jumble into something tidy.",
        ["what is ordering numbers good for"])

    return facts


if __name__ == "__main__":
    from tools.braino.facts import report
    report("numbers", build())
