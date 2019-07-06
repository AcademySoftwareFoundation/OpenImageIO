from docutils import nodes, transforms
from docutils.parsers.rst import Directive
from docutils.parsers.rst import directives

from sphinx import addnodes
from sphinx.domains import Domain, ObjType
from sphinx.locale import l_, _
from sphinx.roles import XRefRole

from pybtex.database.input import bibtex
import pybtex.style.names.plain
import pybtex.style.names.lastfirst
import pybtex.backends.plaintext

import collections
import latex_codec
import os
import re

from backports import OrderedSet

# fix pybtex bug in some versions
try:
    import pybtex.utils

    def _fixed_pybtex_get(self, item, default=None):
        """A case insensitive get."""
        try:
            return self[self._keys[item.lower()]]
        except KeyError:
            return default

    pybtex.utils.CaseInsensitiveDict.get = _fixed_pybtex_get
except:
    pass


latex_codec.register()

DEFAULT_CONF = {
  'file':           '',
  'brackets':       '()',
  'separator':      ';',
  'style':          'authoryear', # 'numbers', 'super'
  'sort':           False,
  'sort_compress':  False
}

ROLES = [
  'p', 'ps', 'alp', 'alps',
  't', 'ts', 'alt', 'alts',
  'author', 'authors', 'year', 'yearpar', 'text', 'title'
]

SUBSUP_RE = re.compile(r'([\s\S^\^\_]*)([\^\_]){?([\S\s^}]*)}?')
def latex_to_nodes(text):
  """
  Convert '_{}' and '^{}' text to node representation.
  TODO: This doesn't work yet
  """
  return nodes.inline(text, text)
  m = SUBSUP_RE.findall(text)
  if m:
    n += latex_to_nodes(m[0][0])
    if m[0][1] == '^':
      n += nodes.superscript(latex_to_nodes(m[0][2]))
    elif m[0][1] == '_':
      n += nodes.subscript(latex_to_nodes(m[0][2]))
  else:
    n = nodes.inline(text, text)
  return n

def parse_keys(rawtext):
  # Get the keys and any pre- and post-ciation text
  # Spaces nor commas are allowed in cite keys, so we split on commas
  # first.  This will give us a list of keys, however, the last item may have
  # pre- and post-citation text (in brackets "[pre][post]")
  #
  # TODO: This isn't the best implementation and this should also
  #       handle errors
  pre = u''
  post = u''
  keys = []
  for k in rawtext.split(','):
    k = k.strip() # Remove leading and trailing whitespace
    k = k.split(' ', 1) # Split on the first space in the key, if any
    if len(k) > 1:
      # We have some extra text here
      k, text = k
      bo, bc = 0, 0
      for c in text.strip():
        if c == "[" and bo == bc:
          bo += 1
        elif c == "]" and bo == bc + 1:
          bc += 1
        else:
          if bc == 0:
            pre += c
          else:
            post += c
      if bc == bo and bc == 1:
        post = pre
        pre = u''
    else:
      k = k[0]
    keys.append(k)
  return (keys, pre, post)


class Citations(object):
  def __init__(self, env):
    self.conf = DEFAULT_CONF.copy()
    self.conf.update(env.config.natbib)

    self.file_name = None
    self.parser = None
    self.data = None
    self.ref_map = {}

    file_name = self.conf.get('file')
    if file_name:
      self.file_name = file_name
      self.parser = bibtex.Parser()
      self.data = self.parser.parse_file(self.file_name)

  def get(self, key):
    return self.data.entries.get(key)

class CitationTransform(object):
  """
  This class is meant to be applied to a ``docutils.nodes.pending`` node when a
  ``cite`` role is encountered.  Later (during the resolve_xref stage) this
  class can be used to generate the proper citation reference nodes for
  insertion into the actual document.
  """
  def __init__(self, refs, pre, post, typ, global_keys, config):
    self.refs = refs
    self.pre = pre
    self.post = post
    self.config = config
    self.typ = typ
    self.global_keys = global_keys

  def __repr__(self):
    return '<%s>' % self.__str__()

  def __str__(self):
    return ','.join([r.key for r in self.refs])

  def get_ref_num(self, key):
    for i, k in enumerate(self.global_keys):
      if k == key:
        return i + 1

  def get_author(self, authors, all_authors=False):
    if len(authors) == 0:
      author = ''
    elif len(authors) > 2 and not all_authors:
      author = u'%s et al.' % authors[0].last()[0]
    else:
      author = u"%s and %s" % (u', '.join([a.last()[0] for a in authors[:-1]]),
                                          authors[-1].last()[0])
    author = author.replace('{', '')
    author = author.replace('}', '')
    return author

  def cite(self, cmd, refuri, global_keys=None):
    """
    Return a docutils Node consisting of properly formatted citations children
    nodes.
    """
    if global_keys is not None:
        self.global_keys = global_keys
    bo, bc = self.config['brackets']
    sep = u'%s ' % self.config['separator']
    style = self.config['style']
    all_auths = (cmd.endswith('s'))
    alt = (cmd.startswith('alt') or \
                (cmd.startswith('alp')) or \
                (style == 'citeyear'))

    if (cmd.startswith('p') or cmd == 'yearpar') and style != 'super':
      node = nodes.inline(bo, bo, classes=['citation'])
    else:
      node = nodes.inline('', '', classes=['citation'])

    if self.pre:
      pre = u"%s " % self.pre.decode('latex')
      node += nodes.inline(pre, pre, classes=['pre'])

    for i, ref in enumerate(self.refs):
      authors = ref.persons.get('author', [])
      author_text = self.get_author(authors, all_auths).decode('latex')
      lrefuri = refuri + '#citation-' + nodes.make_id(ref.key)

      if i > 0 and i < len(self.refs):
        if style == "authoryear":
          node += nodes.inline(sep, sep)
        else:
          if style == "super":
            node += nodes.superscript(', ', ', ')
          else:
            node += nodes.inline(', ', ', ')

      if cmd == 'title':
          title = ref.fields.get('title')
          if title is None:
              title = ref.fields.get('key', '')
          author_text = title

      if (style == "authoryear" and (cmd.startswith('p') or cmd.startswith('alp'))) or \
         (cmd.startswith('t') or cmd.startswith('alt') or cmd.startswith('author')):
        node += nodes.reference(author_text, author_text, internal=True, refuri=lrefuri)

        if cmd.startswith('p') or cmd.startswith('alp'):
          node += nodes.inline(', ', ', ')
        else:
          node += nodes.inline(' ', ' ')

      # Add in either the year or the citation number
      if cmd == 'title':
          pass
      elif cmd.startswith('author'):
          pass
      else:
        if style != 'authoryear':
          num = self.get_ref_num(ref.key)
        else:
          num = ref.fields.get('year')

        refnode = nodes.reference(str(num), str(num), internal=True, refuri=lrefuri)

        if cmd.startswith('t') and style != 'super':
          node += nodes.inline(bo, bo)

        if style == 'super':
          node += nodes.superscript('', '', refnode)
        else:
          node += refnode

        if cmd.startswith('t') and style != 'super':
          node += nodes.inline(bc, bc)

    if self.post:
      post = u", %s" % self.post.decode('latex')
      node += nodes.inline(post, post, classes=['post'])

    if (cmd.startswith('p') or cmd == 'yearpar') and style != 'super':
      node += nodes.inline(bc, bc, classes=['citation'])

    return node


def sort_references(refs, citations):
  def sortkey(key):
    # sort by author last names, but if no author, sort by title
    citation = citations.get(key)
    authorsort = u''.join(map(unicode, citation.persons.get('author', '')))
    if len(authorsort) > 0:
        authorsort = authorsort.replace('{', '')
        authorsort = authorsort.replace('}', '')
        return authorsort.upper()
    titlesort = citation.fields.get('title', '')
    titlesort = titlesort.replace('{', '')
    titlesort = titlesort.replace('}', '')
    return titlesort.upper()
  sortedrefs = sorted(refs, key=sortkey)
  return OrderedSet(sortedrefs)

class CitationXRefRole(XRefRole):
  def __call__(self, typ, rawtext, text, lineno, inliner, options={}, \
                content=[]):
    """
    When a ``cite`` role is encountered, we replace it with a
    ``docutils.nodes.pending`` node that uses a ``CitationTrasform`` for
    generating the proper citation reference representation during the
    resolve_xref phase.
    """
    rnodes = super(CitationXRefRole, self).__call__(typ, rawtext, text, lineno,
                    inliner, options, content)
    rootnode = rnodes[0][0]

    env = inliner.document.settings.env
    citations = env.domains['cite'].citations

    # Get the config at this point in the document
    config = {}
    for opt in ['style', 'brackets', 'separator', 'sort', 'sort_compress']:
      config[opt] = env.temp_data.get("cite_%s" % opt, env.domaindata['cite']['conf'].get(opt, DEFAULT_CONF[opt]))

    if typ == "cite:text":
      # A ``text`` citation is unique because it doesn't reference a cite-key
      keys = []
      pre, post = text, ''
    else:
      keys, pre, post = parse_keys(text)
      for key in keys:
        if citations.get(key) is None:
          env.warn(env.docname, "cite-key `%s` not found in bibtex file" % key, lineno)
          continue
        env.domaindata['cite']['keys'].add(key)
        env.domaindata['cite']['keys'] = sort_references(env.domaindata['cite']['keys'], citations)

    data = {'keys':         keys,
            'pre':          pre,
            'post':         post,
            'typ':          typ,
            'global_keys':  env.domaindata['cite']['keys'],
            'config':       config}

    rootnode += nodes.pending(CitationTransform, data)
    return [rootnode], []

class CitationConfDirective(Directive):
  """
  Allows the user to change the citation style on a per-page or
  per-block basis.
  """
  has_content = False
  required_arguments = 0
  optional_arguments = 1
  option_spec = {
    'brackets':       directives.unchanged,
    'separator':      directives.unchanged,
    'style':          directives.unchanged,
    'sort':           directives.flag,
    'sort_compress':  directives.flag,
  }

  def run(self):
    env = self.state.document.settings.env

    # TODO: verify options
    if self.arguments:
      env.temp_data['cite_style'] = self.arguments[0]
    else:
      env.temp_data['cite_style'] = self.options.get('style', DEFAULT_CONF['style'])

    try:
      self.options.pop('style')
    except:
      pass

    for k, v in self.options.items():
      env.temp_data['cite_%s' % k] = v

    return []

class CitationReferencesDirective(Directive):
  """
  Generates the actual reference list.
  """
  has_content = False
  required_arguments = 0
  optional_arguments = 0

  # TODO: Implement support for multiple bib files
  option_spec = {
    'path': directives.unchanged,
  }

  def get_reference_node(self, ref):
    node = nodes.inline('', '', classes=[ref.type, 'reference'])

    namestyler = pybtex.style.names.plain.NameStyle()
    plaintext = pybtex.backends.plaintext.Backend()

    # Authors
    authors = ref.persons.get('author', [])
    for i, author in enumerate(authors):
      authortext = namestyler.format(author, abbr=True).format().render(plaintext)
      authortext = authortext.replace('{', '')
      authortext = authortext.replace('}', '')
      authortext = authortext.decode('latex')
      text = authortext

      text = text.strip()
      auth_node = latex_to_nodes(text)
      auth_node['classes'].append('author')
      node += auth_node

      if i + 1 < len(authors):
        node += nodes.inline(', ', ', ')
      else:
        ending = '%s  ' % ('' if text.endswith('.') else '.')
        node += nodes.inline(ending, ending)

    # Title
    title = ref.fields.get('title')
    if title is None:
        title = ref.fields.get('key')
    if title:
      title = title.decode('latex')
      title = title.replace('{', '')
      title = title.replace('}', '')
      node += nodes.inline(title, title, classes=['title'])
      node += nodes.inline('.  ', '.  ')

    # @phdthesis
    if ref.type == 'phdthesis':
        school = ref.fields.get('school')
        school = school.decode('latex')
        text = 'PhD Thesis, %s, ' % school
        node += nodes.inline(text, text)

    # Publication
    pub = ref.fields.get('journal')
    if not pub:
        pub = ref.fields.get('booktitle')
    if pub:
      pub = pub.decode('latex')
      pub = pub.replace('{', '')
      pub = pub.replace('}', '')
      node += nodes.emphasis(pub, pub, classes=['publication'])
      node += nodes.inline(', ', ', ')

    vol = ref.fields.get('volume')
    pages = ref.fields.get('pages')
    year = ref.fields.get('year')

    if pub is None:
      howpub = ref.fields.get('howpublished')
      if howpub is not None and howpub.startswith('\url{'):
        url = howpub[5:-1]
        refnode = nodes.reference('', '', internal=False, refuri=url)
        refnode += nodes.Text(url, url)
        node += refnode
        if vol or pages or year:
          node += nodes.inline(', ', ', ')

    if vol:
      vol = vol.decode('latex')
      node += nodes.inline(vol, vol, classes=['volume'])
      node += nodes.inline(':', ':')

    if pages:
      pages = pages.decode('latex')
      node += nodes.inline(pages, pages, classes=['pages'])
      node += nodes.inline(', ', ', ')

    if year:
      year = year.decode('latex')
      node += nodes.inline(year, year, classes=['year'])
      node += nodes.inline('.', '.')

    return node

  def run(self):
    """
    Generate the definition list that displays the actual references.
    """
    env = self.state.document.settings.env
    keys = env.domaindata['cite']['keys']
    env.domaindata['cite']['refdoc'] = env.docname

    citations = env.domains['cite'].citations

    # TODO: implement
    #env.domaindata['cite']['refdocs'][env.docname] = Citations(env, path)

    tbody = nodes.tbody('')
    for i, key in enumerate(keys):
        row = nodes.row('')
        nid = "citation-%s" % nodes.make_id(key)
        row['classes'].append('footnote')
        row['ids'].append(nid)
        row['names'].append(nid)

        numcol = nodes.entry('', nodes.paragraph('', '[%d]' % (i + 1)))
        definition = self.get_reference_node(citations.get(key))
        refcol = nodes.entry('', nodes.paragraph('', '', definition))
        row.extend([numcol, refcol])

        tbody.append(row)

    table_spec_node = addnodes.tabular_col_spec()
    table_spec_node['spec'] = 'cl'

    node = nodes.table('',
                       table_spec_node,
                       nodes.tgroup('',
                                    nodes.colspec(colwidth=10, classes=['label']),
                                    nodes.colspec(colwidth=90),
                                    tbody))

    return [node]

class CitationDomain(Domain):
  name = "cite"
  label = "citation"

  object_types = {
    'citation': ObjType(l_('citation'), *ROLES, searchprio= -1),
  }

  directives = {
    'conf': CitationConfDirective,
    'refs': CitationReferencesDirective
  }
  roles = dict([(r, CitationXRefRole()) for r in ROLES])

  initial_data = {
    'keys':     OrderedSet(), # Holds cite-keys in order of reference
    'conf':     DEFAULT_CONF,
    'refdocs':  {}
  }

  def __init__(self, env):
    super(CitationDomain, self).__init__(env)

    # Update conf
    env.domaindata['cite']['conf'].update(env.config.natbib)

    # TODO: warn if citations can't parse bibtex file
    self.citations = Citations(env)

  def resolve_xref(self, env, fromdocname, builder,
                       typ, target, node, contnode):

    refdoc = env.domaindata['cite'].get('refdoc')
    if not refdoc:
      env.warn(fromdocname  , 'no `refs` directive found; citations will have dead links', node.line)
      refuri = ''
    else:
      refuri = builder.get_relative_uri(fromdocname, refdoc)

    for nd in node.children:
      if isinstance(nd, nodes.pending):
        nd.details['refs'] = []

        if builder.name == 'latex':
            cite_keys = nd.details['keys']
            cite_keys = ','.join(cite_keys)
            cite_node = nodes.citation_reference(cite_keys, cite_keys)
            return cite_node

        for key in nd.details.pop('keys'):
          ref = self.citations.get(key)
          if ref is None:
            continue
          nd.details['refs'].append(ref)

        transform = nd.transform(**nd.details)
        node = transform.cite(typ, refuri, global_keys=env.domaindata['cite']['keys'])

    return node

def setup(app):
  app.add_config_value('natbib', DEFAULT_CONF, 'env')
  app.add_domain(CitationDomain)
