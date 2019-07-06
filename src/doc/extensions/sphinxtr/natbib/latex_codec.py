"""latex.py

Character translation utilities for LaTeX-formatted text.

Usage:
 - unicode(string,'latex')
 - ustring.decode('latex')
are both available just by letting "import latex" find this file.
 - unicode(string,'latex+latin1')
 - ustring.decode('latex+latin1')
where latin1 can be replaced by any other known encoding, also
become available by calling latex.register().

We also make public a dictionary latex_equivalents,
mapping ord(unicode char) to LaTeX code.

D. Eppstein, October 2003.
"""

from __future__ import generators
import codecs
import re
from backports import Set

def register():
    """Enable encodings of the form 'latex+x' where x describes another encoding.
    Unicode characters are translated to or from x when possible, otherwise
    expanded to latex.
    """
    codecs.register(_registry)

def getregentry():
    """Encodings module API."""
    return _registry('latex')

def _registry(encoding):
    if encoding == 'latex':
        encoding = None
    elif encoding.startswith('latex+'):
        encoding = encoding[6:]
    else:
        return None

    class Codec(codecs.Codec):
        def encode(self,input,errors='strict'):
            """Convert unicode string to latex."""
            output = []
            for c in input:
                if encoding:
                    try:
                        output.append(c.encode(encoding))
                        continue
                    except:
                        pass
                if ord(c) in latex_equivalents:
                    output.append(latex_equivalents[ord(c)])
                else:
                    output += ['{\\char', str(ord(c)), '}']
            return ''.join(output), len(input)

        def decode(self,input,errors='strict'):
            """Convert latex source string to unicode."""
            if encoding:
                input = unicode(input,encoding,errors)

            # Note: we may get buffer objects here.
            # It is not permussable to call join on buffer objects
            # but we can make them joinable by calling unicode.
            # This should always be safe since we are supposed
            # to be producing unicode output anyway.
            x = map(unicode,_unlatex(input))
            return u''.join(x), len(input)

    class StreamWriter(Codec,codecs.StreamWriter):
        pass

    class StreamReader(Codec,codecs.StreamReader):
        pass

    return (Codec().encode,Codec().decode,StreamReader,StreamWriter)

def _tokenize(tex):
    """Convert latex source into sequence of single-token substrings."""
    start = 0
    try:
        # skip quickly across boring stuff
        pos = _stoppers.finditer(tex).next().span()[0]
    except StopIteration:
        yield tex
        return

    while 1:
        if pos > start:
            yield tex[start:pos]
            if tex[start] == '\\' and not (tex[pos-1].isdigit() and tex[start+1].isalpha()):
                while pos < len(tex) and tex[pos].isspace(): # skip blanks after csname
                    pos += 1

        while pos < len(tex) and tex[pos] in _ignore:
            pos += 1    # flush control characters
        if pos >= len(tex):
            return
        start = pos
        if tex[pos:pos+2] in {'$$':None, '/~':None}:    # protect ~ in urls
            pos += 2
        elif tex[pos].isdigit():
            while pos < len(tex) and tex[pos].isdigit():
                pos += 1
        elif tex[pos] == '-':
            while pos < len(tex) and tex[pos] == '-':
                pos += 1
        elif tex[pos] != '\\' or pos == len(tex) - 1:
            pos += 1
        elif not tex[pos+1].isalpha():
            pos += 2
        else:
            pos += 1
            while pos < len(tex) and tex[pos].isalpha():
                pos += 1
            if tex[start:pos] == '\\char' or tex[start:pos] == '\\accent':
                while pos < len(tex) and tex[pos].isdigit():
                    pos += 1

class _unlatex:
    """Convert tokenized tex into sequence of unicode strings.  Helper for decode()."""

    def __iter__(self):
        """Turn self into an iterator.  It already is one, nothing to do."""
        return self

    def __init__(self,tex):
        """Create a new token converter from a string."""
        self.tex = tuple(_tokenize(tex))  # turn tokens into indexable list
        self.pos = 0                    # index of first unprocessed token
        self.lastoutput = 'x'           # lastoutput must always be nonempty string

    def __getitem__(self,n):
        """Return token at offset n from current pos."""
        p = self.pos + n
        t = self.tex
        return p < len(t) and t[p] or None

    def next(self):
        """Find and return another piece of converted output."""
        if self.pos >= len(self.tex):
            raise StopIteration
        nextoutput = self.chunk()
        if self.lastoutput[0] == '\\' and self.lastoutput[-1].isalpha() and nextoutput[0].isalpha():
            nextoutput = ' ' + nextoutput   # add extra space to terminate csname
        self.lastoutput = nextoutput
        return nextoutput

    def chunk(self):
        """Grab another set of input tokens and convert them to an output string."""
        for delta,c in self.candidates(0):
            if c in _l2u:
                self.pos += delta
                return unichr(_l2u[c])
            elif len(c) == 2 and c[1] == 'i' and (c[0],'\\i') in _l2u:
                self.pos += delta       # correct failure to undot i
                return unichr(_l2u[(c[0],'\\i')])
            elif len(c) == 1 and c[0].startswith('\\char') and c[0][5:].isdigit():
                self.pos += delta
                return unichr(int(c[0][5:]))

        # nothing matches, just pass through token as-is
        self.pos += 1
        return self[-1]

    def candidates(self,offset):
        """Generate pairs delta,c where c is a token or tuple of tokens from tex
        (after deleting extraneous brackets starting at pos) and delta
        is the length of the tokens prior to bracket deletion.
        """
        t = self[offset]
        if t in _blacklist:
            return
        elif t == '{':
            for delta,c in self.candidates(offset+1):
                if self[offset+delta+1] == '}':
                    yield delta+2,c
        elif t == '\\mbox':
            for delta,c in self.candidates(offset+1):
                yield delta+1,c
        elif t == '$' and self[offset+2] == '$':
            yield 3, (t,self[offset+1],t)
        else:
            q = self[offset+1]
            if q == '{' and self[offset+3] == '}':
                yield 4, (t,self[offset+2])
            elif q:
                yield 2, (t,q)
            yield 1, t

latex_equivalents = {
    0x0009: ' ',
    0x000a: '\n',
    0x0023: '{\#}',
    0x0026: '{\&}',
    0x00a0: '{~}',
    0x00a1: '{!`}',
    0x00a2: '{\\not{c}}',
    0x00a3: '{\\pounds}',
    0x00a7: '{\\S}',
    0x00a8: '{\\"{}}',
    0x00a9: '{\\copyright}',
    0x00af: '{\\={}}',
    0x00ac: '{\\neg}',
    0x00ad: '{\\-}',
    0x00b0: '{\\mbox{$^\\circ$}}',
    0x00b1: '{\\mbox{$\\pm$}}',
    0x00b2: '{\\mbox{$^2$}}',
    0x00b3: '{\\mbox{$^3$}}',
    0x00b4: "{\\'{}}",
    0x00b5: '{\\mbox{$\\mu$}}',
    0x00b6: '{\\P}',
    0x00b7: '{\\mbox{$\\cdot$}}',
    0x00b8: '{\\c{}}',
    0x00b9: '{\\mbox{$^1$}}',
    0x00bf: '{?`}',
    0x00c0: '{\\`A}',
    0x00c1: "{\\'A}",
    0x00c2: '{\\^A}',
    0x00c3: '{\\~A}',
    0x00c4: '{\\"A}',
    0x00c5: '{\\AA}',
    0x00c6: '{\\AE}',
    0x00c7: '{\\c{C}}',
    0x00c8: '{\\`E}',
    0x00c9: "{\\'E}",
    0x00ca: '{\\^E}',
    0x00cb: '{\\"E}',
    0x00cc: '{\\`I}',
    0x00cd: "{\\'I}",
    0x00ce: '{\\^I}',
    0x00cf: '{\\"I}',
    0x00d1: '{\\~N}',
    0x00d2: '{\\`O}',
    0x00d3: "{\\'O}",
    0x00d4: '{\\^O}',
    0x00d5: '{\\~O}',
    0x00d6: '{\\"O}',
    0x00d7: '{\\mbox{$\\times$}}',
    0x00d8: '{\\O}',
    0x00d9: '{\\`U}',
    0x00da: "{\\'U}",
    0x00db: '{\\^U}',
    0x00dc: '{\\"U}',
    0x00dd: "{\\'Y}",
    0x00df: '{\\ss}',
    0x00e0: '{\\`a}',
    0x00e1: "{\\'a}",
    0x00e2: '{\\^a}',
    0x00e3: '{\\~a}',
    0x00e4: '{\\"a}',
    0x00e5: '{\\aa}',
    0x00e6: '{\\ae}',
    0x00e7: '{\\c{c}}',
    0x00e8: '{\\`e}',
    0x00e9: "{\\'e}",
    0x00ea: '{\\^e}',
    0x00eb: '{\\"e}',
    0x00ec: '{\\`\\i}',
    0x00ed: "{\\'\\i}",
    0x00ee: '{\\^\\i}',
    0x00ef: '{\\"\\i}',
    0x00f1: '{\\~n}',
    0x00f2: '{\\`o}',
    0x00f3: "{\\'o}",
    0x00f4: '{\\^o}',
    0x00f5: '{\\~o}',
    0x00f6: '{\\"o}',
    0x00f7: '{\\mbox{$\\div$}}',
    0x00f8: '{\\o}',
    0x00f9: '{\\`u}',
    0x00fa: "{\\'u}",
    0x00fb: '{\\^u}',
    0x00fc: '{\\"u}',
    0x00fd: "{\\'y}",
    0x00ff: '{\\"y}',

    0x0100: '{\\=A}',
    0x0101: '{\\=a}',
    0x0102: '{\\u{A}}',
    0x0103: '{\\u{a}}',
    0x0104: '{\\c{A}}',
    0x0105: '{\\c{a}}',
    0x0106: "{\\'C}",
    0x0107: "{\\'c}",
    0x0108: "{\\^C}",
    0x0109: "{\\^c}",
    0x010a: "{\\.C}",
    0x010b: "{\\.c}",
    0x010c: "{\\v{C}}",
    0x010d: "{\\v{c}}",
    0x010e: "{\\v{D}}",
    0x010f: "{\\v{d}}",
    0x0112: '{\\=E}',
    0x0113: '{\\=e}',
    0x0114: '{\\u{E}}',
    0x0115: '{\\u{e}}',
    0x0116: '{\\.E}',
    0x0117: '{\\.e}',
    0x0118: '{\\c{E}}',
    0x0119: '{\\c{e}}',
    0x011a: "{\\v{E}}",
    0x011b: "{\\v{e}}",
    0x011c: '{\\^G}',
    0x011d: '{\\^g}',
    0x011e: '{\\u{G}}',
    0x011f: '{\\u{g}}',
    0x0120: '{\\.G}',
    0x0121: '{\\.g}',
    0x0122: '{\\c{G}}',
    0x0123: '{\\c{g}}',
    0x0124: '{\\^H}',
    0x0125: '{\\^h}',
    0x0128: '{\\~I}',
    0x0129: '{\\~\\i}',
    0x012a: '{\\=I}',
    0x012b: '{\\=\\i}',
    0x012c: '{\\u{I}}',
    0x012d: '{\\u\\i}',
    0x012e: '{\\c{I}}',
    0x012f: '{\\c{i}}',
    0x0130: '{\\.I}',
    0x0131: '{\\i}',
    0x0132: '{IJ}',
    0x0133: '{ij}',
    0x0134: '{\\^J}',
    0x0135: '{\\^\\j}',
    0x0136: '{\\c{K}}',
    0x0137: '{\\c{k}}',
    0x0139: "{\\'L}",
    0x013a: "{\\'l}",
    0x013b: "{\\c{L}}",
    0x013c: "{\\c{l}}",
    0x013d: "{\\v{L}}",
    0x013e: "{\\v{l}}",
    0x0141: '{\\L}',
    0x0142: '{\\l}',
    0x0143: "{\\'N}",
    0x0144: "{\\'n}",
    0x0145: "{\\c{N}}",
    0x0146: "{\\c{n}}",
    0x0147: "{\\v{N}}",
    0x0148: "{\\v{n}}",
    0x014c: '{\\=O}',
    0x014d: '{\\=o}',
    0x014e: '{\\u{O}}',
    0x014f: '{\\u{o}}',
    0x0150: '{\\H{O}}',
    0x0151: '{\\H{o}}',
    0x0152: '{\\OE}',
    0x0153: '{\\oe}',
    0x0154: "{\\'R}",
    0x0155: "{\\'r}",
    0x0156: "{\\c{R}}",
    0x0157: "{\\c{r}}",
    0x0158: "{\\v{R}}",
    0x0159: "{\\v{r}}",
    0x015a: "{\\'S}",
    0x015b: "{\\'s}",
    0x015c: "{\\^S}",
    0x015d: "{\\^s}",
    0x015e: "{\\c{S}}",
    0x015f: "{\\c{s}}",
    0x0160: "{\\v{S}}",
    0x0161: "{\\v{s}}",
    0x0162: "{\\c{T}}",
    0x0163: "{\\c{t}}",
    0x0164: "{\\v{T}}",
    0x0165: "{\\v{t}}",
    0x0168: "{\\~U}",
    0x0169: "{\\~u}",
    0x016a: "{\\=U}",
    0x016b: "{\\=u}",
    0x016c: "{\\u{U}}",
    0x016d: "{\\u{u}}",
    0x016e: "{\\r{U}}",
    0x016f: "{\\r{u}}",
    0x0170: "{\\H{U}}",
    0x0171: "{\\H{u}}",
    0x0172: "{\\c{U}}",
    0x0173: "{\\c{u}}",
    0x0174: "{\\^W}",
    0x0175: "{\\^w}",
    0x0176: "{\\^Y}",
    0x0177: "{\\^y}",
    0x0178: '{\\"Y}',
    0x0179: "{\\'Z}",
    0x017a: "{\\'Z}",
    0x017b: "{\\.Z}",
    0x017c: "{\\.Z}",
    0x017d: "{\\v{Z}}",
    0x017e: "{\\v{z}}",

    0x01c4: "{D\\v{Z}}",
    0x01c5: "{D\\v{z}}",
    0x01c6: "{d\\v{z}}",
    0x01c7: "{LJ}",
    0x01c8: "{Lj}",
    0x01c9: "{lj}",
    0x01ca: "{NJ}",
    0x01cb: "{Nj}",
    0x01cc: "{nj}",
    0x01cd: "{\\v{A}}",
    0x01ce: "{\\v{a}}",
    0x01cf: "{\\v{I}}",
    0x01d0: "{\\v\\i}",
    0x01d1: "{\\v{O}}",
    0x01d2: "{\\v{o}}",
    0x01d3: "{\\v{U}}",
    0x01d4: "{\\v{u}}",
    0x01e6: "{\\v{G}}",
    0x01e7: "{\\v{g}}",
    0x01e8: "{\\v{K}}",
    0x01e9: "{\\v{k}}",
    0x01ea: "{\\c{O}}",
    0x01eb: "{\\c{o}}",
    0x01f0: "{\\v\\j}",
    0x01f1: "{DZ}",
    0x01f2: "{Dz}",
    0x01f3: "{dz}",
    0x01f4: "{\\'G}",
    0x01f5: "{\\'g}",
    0x01fc: "{\\'\\AE}",
    0x01fd: "{\\'\\ae}",
    0x01fe: "{\\'\\O}",
    0x01ff: "{\\'\\o}",

    0x02c6: '{\\^{}}',
    0x02dc: '{\\~{}}',
    0x02d8: '{\\u{}}',
    0x02d9: '{\\.{}}',
    0x02da: "{\\r{}}",
    0x02dd: '{\\H{}}',
    0x02db: '{\\c{}}',
    0x02c7: '{\\v{}}',

    0x03c0: '{\\mbox{$\\pi$}}',
    # consider adding more Greek here

    0xfb01: '{fi}',
    0xfb02: '{fl}',

    0x2013: '{--}',
    0x2014: '{---}',
    0x2018: "{`}",
    0x2019: "{'}",
    0x201c: "{``}",
    0x201d: "{''}",
    0x2020: "{\\dag}",
    0x2021: "{\\ddag}",
    0x2122: "{\\mbox{$^\\mbox{TM}$}}",
    0x2022: "{\\mbox{$\\bullet$}}",
    0x2026: "{\\ldots}",
    0x2202: "{\\mbox{$\\partial$}}",
    0x220f: "{\\mbox{$\\prod$}}",
    0x2211: "{\\mbox{$\\sum$}}",
    0x221a: "{\\mbox{$\\surd$}}",
    0x221e: "{\\mbox{$\\infty$}}",
    0x222b: "{\\mbox{$\\int$}}",
    0x2248: "{\\mbox{$\\approx$}}",
    0x2260: "{\\mbox{$\\neq$}}",
    0x2264: "{\\mbox{$\\leq$}}",
    0x2265: "{\\mbox{$\\geq$}}",
}
for _i in range(0x0020):
    if _i not in latex_equivalents:
        latex_equivalents[_i] = ''
for _i in range(0x0020,0x007f):
    if _i not in latex_equivalents:
        latex_equivalents[_i] = chr(_i)

# Characters that should be ignored and not output in tokenization
_ignore = Set([chr(i) for i in range(32)+[127]]) - Set('\t\n\r')

# Regexp of chars not in blacklist, for quick start of tokenize
_stoppers = re.compile('[\x00-\x1f!$\\-?\\{~\\\\`\']')

_blacklist = Set(' \n\r')
_blacklist.add(None)    # shortcut candidate generation at end of data

# Construction of inverse translation table
_l2u = {
    '\ ':ord(' ')   # unexpanding space makes no sense in non-TeX contexts
}

for _tex in latex_equivalents:
    if _tex <= 0x0020 or (_tex <= 0x007f and len(latex_equivalents[_tex]) <= 1):
        continue    # boring entry
    _toks = tuple(_tokenize(latex_equivalents[_tex]))
    if _toks[0] == '{' and _toks[-1] == '}':
        _toks = _toks[1:-1]
    if _toks[0].isalpha():
        continue    # don't turn ligatures into single chars
    if len(_toks) == 1 and (_toks[0] == "'" or _toks[0] == "`"):
        continue    # don't turn ascii quotes into curly quotes
    if _toks[0] == '\\mbox' and _toks[1] == '{' and _toks[-1] == '}':
        _toks = _toks[2:-1]
    if len(_toks) == 4 and _toks[1] == '{' and _toks[3] == '}':
        _toks = (_toks[0],_toks[2])
    if len(_toks) == 1:
        _toks = _toks[0]
    _l2u[_toks] = _tex

# Shortcut candidate generation for certain useless candidates:
# a character is in _blacklist if it can not be at the start
# of any translation in _l2u.  We use this to quickly skip through
# such characters before getting to more difficult-translate parts.
# _blacklist is defined several lines up from here because it must
# be defined in order to call _tokenize, however it is safe to
# delay filling it out until now.

for i in range(0x0020,0x007f):
    _blacklist.add(chr(i))
_blacklist.remove('{')
_blacklist.remove('$')
for candidate in _l2u:
    if isinstance(candidate,tuple):
        if not candidate or not candidate[0]:
            continue
        firstchar = candidate[0][0]
    else:
        firstchar = candidate[0]
    _blacklist.discard(firstchar)
