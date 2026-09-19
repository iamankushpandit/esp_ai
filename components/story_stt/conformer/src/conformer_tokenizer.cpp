#include "conformer_tokenizer.h"

#include <array>

namespace conformer
{
    // Blank token for CTC-decoding
    static constexpr uint32_t kBlankToken{1024};

    // Vocabulary of the conformer
    static constexpr std::array<const std::string, 1024> kVocabulary{
        " ⁇ ", "s", " the", " a", "t", " to", " and", " i", " of", "\'", "ed", " in", "d", "ing",
        "n", "e", " it", " that", " you", "y", "er", "r", " for", "m", " is", " he", "re", " was",
        " be", "p", "ly", " so", " we", "a", "g", "o", " c", "b", "u", " on", " have", " but", "ll",
        " with", " re", "or", " s", "al", " do", " know", "ar", " they", " not", " as", " this",
        "in", "le", " e", " are", " like", "c", " uh", "ri", " me", " his", " at", "l", "es", " de",
        " yeah", " can", "k", " or", " my", " all", " had", " there", " will", " one", "il", " no",
        " what", "en", "ck", " b", " f", "ce", "ch", "i", " by", " she", " from", " an", "ic", "ur",
        "ve", "w", "ter", "la", " if", " just", "th", "li", " ", " her", " um", "on", "ation", " w",
        " would", "f", "te", " st", " go", "ir", "it", " out", "ro", " pa", " were", " g", " t", "ion",
        " think", "an", " right", " about", "se", "lo", "ent", " up", "ment", "ate", " when", "h",
        "ne", " don", " has", " also", " more", " see", " okay", " their", " your", "ge", " who",
        " well", " co", " which", " some", " se", " time", " ba", " said", " con", "ers", " ra",
        "us", "de", "ra", " him", " our", " been", " fa", " po", " pro", "et", "x", " la", "id",
        "ver", " oh", " ma", "v", " now", "age", " two", "ld", " mo", " how", "tion", " people",
        "ive", " other", "ng", "ity", "z", "ist", " very", " get", " any", " un", " ro", "is",
        " work", " mean", " them", " lo", "vi", " because", "ies", "ul", "as", "ad", "mp", " bo",
        "-", " then", " good", "el", "nd", " li", " man", " dis", " could", " ho", "at", "ol", " bu",
        " te", " ha", "est", "me", " say", "ru", "ke", " sp", " k", "able", " su", " sa", " di",
        " fi", "ance", " really", " over", " even", "ry", " us", " ca", "ow", "ho", " into", "ence",
        "mo", " mi", "one", "qu", "ut", "lu", " o", "ty", " after", " want", " new", " take", " p",
        " look", " pre", "sh", " day", " should", " th", " need", " cha", "co", " much", " where",
        " d", "ant", " fe", " da", " make", "om", " did", " le", "un", " only", "im", " these", "ff",
        "ti", "ish", " ex", "ted", " first", "he", "ig", " vi", " ri", " en", " com", "ated", " than",
        "ma", " way", "um", "ct", "end", "ight", " here", " ta", " car", " part", " come", "ia",
        " off", " sc", " ah", "am", " tra", " yes", " back", "ture", "ful", " pri", "ction", "ine",
        " three", "ard", " let", "pe", " little", " down", "mb", " si", " dr", " mr", " going",
        " comp", "po", " m", " sta", " gra", "day", " many", "ian", "ta", " long", " pi", " too",
        " app", " kind", "ous", "ci", " ga", "ten", "nt", " before", " may", " got", "man", "tic",
        "ition", "cu", "ugh", "tra", " n", "ward", " give", " every", " hi", "ting", " exp", " those",
        " hu", "ot", " something", " lot", " still", " ne", "na", "ise", "pp", " most", " gu",
        " state", " actually", " such", " bi", " never", "tain", " great", " through", " al", "no",
        " mar", " year", "ach", "les", " school", "ally", "ial", "ha", " old", " made", "ary", " ar",
        " years", " help", " per", "ving", "ical", "ther", " does", "ac", "ca", " must", "di", " own",
        " ru", " things", " hand", " thing", " high", " last", "go", " sh", " under", " four", " place",
        "ations", " sure", "mi", "nce", " am", "for", "ness", " name", " five", "ound", " op", " cons",
        " ph", " same", "row", "ven", "ph", "ite", " pe", "j", " sha", " friend", " wi", " call",
        " european", " h", "ect", "ress", " live", "port", " mhm", " house", "ie", "ni", " plan", " jo",
        " play", "side", " va", "min", "ious", " life", " du", " ti", " six", " men", " again",
        " thank", " talk", "par", " home", "op", " both", " why", " put", " another", "nc", " being",
        "mit", " came", "led", " fo", " end", " member", "ative", " thought", " tri", "iv", "our",
        "red", " went", "lic", " find", " pu", "land", " start", " far", " eu", " imp", " always",
        " ju", " wa", " person", " singapore", "ap", " show", " chi", " ten", " eight", " while",
        " point", " y", " ja", " ya", "ling", "ctor", " use", " acc", " world", " pay", " read", "va",
        "vo", " change", " u", " pl", " sw", " war", " might", "nk", "ments", "and", " different",
        " dec", "cent", " ste", " better", " fun", " month", "ship", "ton", " tell", " twenty",
        " commission", " exc", " miss", "if", " love", " money", " found", " hundred", "gg", " add",
        " real", "ities", " na", " pass", " didn", " v", " feel", " week", " win", "ible", " try",
        " upon", "ba", " interest", " inter", "son", "line", " ob", " boy", " big", " used", " seven",
        " away", " family", "less", " ki", "ber", " around", " turn", " anything", " care", " young",
        " guess", " happen", " course", " agree", " support", " conf", "ual", " number", " trans",
        "ating", " mister", " hard", " watch", "ft", " next", " sea", " open", " without", "duc",
        "gra", "ak", " cap", " cre", "hi", " government", " vo", " between", " each", " ve", " though",
        " country", " few", " once", " \"", " head", " free", " mu", " maybe", " act", " night",
        " thousand", " face", " uhhuh", " keep", " nine", " close", " case", " che", " against",
        " done", " ever", " law", " believe", " public", " room", " sub", " order", " important",
        "ient", " el", " children", " second", " bri", " business", " hope", " move", "fa", " however",
        " follow", " able", " word", " yet", " fla", " stand", "ize", " je", " service", " nothing",
        " report", " called", " grow", " continue", " issue", " since", " book", " lu", " qui",
        " develop", " gen", " certain", "light", " cor", " small", " took", " question", " whole",
        " problem", " side", " child", " full", " best", " mm", " probably", "fi", " qua", " sur",
        " market", " left", " everything", " during", " understand", "ook", "wa", " cent", " water",
        " quite", " leave", " himself", "ip", " near", " saw", " together", " large", " having",
        " already", " invest", " pretty", " direct", " hour", " fact", "way", " run", " bra", " clear",
        " fra", " area", " union", " enough", " consider", " lead", " remain", " president",
        " system", " def", " stuff", " food", " job", " heard", " err", " mind", " rest", " speak",
        " asked", "ator", " half", " father", "com", " less", " arm", " human", "ency", " matter",
        " group", " girl", " current", " main", "ttle", " later", " learn", " strong", " sign",
        " check", " light", " else", " true", " term", "qui", " minute", " spec", " return", " answer",
        " reason", " count", " shall", " communi", " travel", " wait", " provide", " low", " mother",
        " expect", " cause", " line", " general", "lf", " getting", " parliament", " bank", " company",
        " stop", "cause", " power", " gi", " europe", " moment", " among", " walk", " allow", " idea",
        " office", " town", " cannot", " countries", " become", " appear", " present", " bring",
        " least", " almost", " kids", " remember", " include", " short", " sometimes", " game",
        " level", " exactly", " particular", " social", " land", " woman", " north", " nice",
        " concern", " sort", " effect", " national", " several", " safe", " until", " further",
        " cost", " wonder", " whether", " either", " future", " pra", " council", " knew",
        " common", " south", " making", " morning", " process", " situation", " white", " result",
        " suppose", " employ", " political", " program", " along", " women", " ski", " court",
        " please", " shi", " possible", " protect", " experience", " definitely", " require",
        " account", " myself", " black", " example", " america", " thirty", " student", " view",
        " product", " wife", " health", " major", " difficult", " death", " visit", " across",
        " receive", " voice", " citizen", " regard", " author", " treat", " especially", " local",
        " taking", " information", " seemed", " success", "ability", " break", " whatever",
        " security", " address", " felt", " fifty", " million", " third", " usually", " gonna",
        " brother", " began", " period", " east", " economic", " increase", " financial", " respect",
        " enjoy", " christ", " education", " brought", " organ", " parents", " policy", " round",
        " became", " region", " lady", " discuss", " single", " early", " couple", " type", " itself",
        " serve", " measure", " husband", "ified", " music", " ground", " companies", " street",
        " behind", " value", " therefore", " police", " complete", " john", " daughter", " affect",
        " perhaps", " international", " themselves", " improve", " condition", " hotel", " deliver",
        " sense", " relation", " sorry", " credit", " effort", " instead", " york", " united",
        " partner", " spoke", " strange", " everybody", " horse", " depend", " subject", " project",
        " approach", " involve", " listen", " draw", " computer", " married", " record", " happy",
        " sudden", " represent", " somebody", " correct", " serious", " decision", " society",
        " including", " college", " english", " attack", " perform", " cross", " accept", " control",
        " flow", " although", " drink", " front", " wrong", " twi", " according", " slow", " peace",
        " amount", " object", " movie", " benefit", " yup", " challenge", " private", " church",
        " wood", " field", " above", " ensure", " immediate", " figure", " foreign", " available",
        " insurance", " proposal", " doubt", " strength", " difference", " stood", " implement",
        " economy", " detail", " umhum", " restaurant", " collect", " global", " broke", "q"};

    void Decode(const tlib::TensorView<uint32_t> &tokens, std::string &buffer)
    {
        // TODO: Could potentially increase accuracy through beam-search
        assert(tokens.Dim() == 1);

        const uint32_t *token_data{tokens.DataImm()};

        uint32_t prev_token{kBlankToken};
        for (uint32_t i = 0; i < tokens.Numel(); i++)
        {
            if ((token_data[i] != prev_token || prev_token == kBlankToken) && token_data[i] != kBlankToken && token_data[i] < kBlankToken)
            {
                buffer.append(kVocabulary.at(token_data[i]));
            }
            prev_token = token_data[i];
        }
    }

} // conformer