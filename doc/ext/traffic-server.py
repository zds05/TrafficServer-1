# -*- coding: utf-8 -*-
"""
    TS Sphinx Directives
    ~~~~~~~~~~~~~~~~~~~~~~~~~

    Sphinx Docs directives for Apache Traffic Server

    :copyright: Copyright 2013 by the Apache Software Foundation
    :license: Apache
"""

from docutils import nodes
from docutils.parsers import rst
from sphinx.domains import Domain, ObjType, std
from sphinx.roles import XRefRole
from sphinx.locale import l_, _
import sphinx

class TSConfVar(std.Target):
    """
    Description of a traffic server configuration variable.

    Argument is the variable as defined in records.config.

    Descriptive text should follow, indented.

    Then the bulk description (if any) undented. This should be considered equivalent to the Doxygen
    short and long description.
    """

    option_spec = {
        'class' : rst.directives.class_option,
        'reloadable' : rst.directives.flag,
        'deprecated' : rst.directives.flag,
    }
    required_arguments = 3
    optional_arguments = 1 # default is optional, special case if omitted
    final_argument_whitespace = True
    has_content = True

    def make_field(self, tag, value):
        field = nodes.field();
        field.append(nodes.field_name(text=tag))
        body = nodes.field_body()
        if (isinstance(value, basestring)):
            body.append(sphinx.addnodes.compact_paragraph(text=value))
        else:
            body.append(value)
        field.append(body)
        return field

    # External entry point
    def run(self):
        env = self.state.document.settings.env
        cv_default = None
        cv_scope, cv_name, cv_type = self.arguments[0:3]
        if (len(self.arguments) > 3):
            cv_default = self.arguments[3]
        title = sphinx.addnodes.desc_name(text=cv_name)
        self.add_name(title)
        title.set_class('ts-cv-title')
        if ('class' in self.options):
            title.set_class(self.options.get('class'))
        # This has to be a distinct node before the title. if nested then
        # the browser will scroll forward to just past the title.
        anchor = nodes.target('', '', names=[cv_name])
        # Second (optional) arg is 'msgNode' - no idea what I should pass for that
        # or if it even matters, although I now think it should not be used.
        self.state.document.note_explicit_target(anchor);
        env.domaindata['ts']['cv'][cv_name] = env.docname

        fl = nodes.field_list()
        fl.append(self.make_field('Scope', cv_scope))
        fl.append(self.make_field('Type', cv_type))
        if (cv_default):
            fl.append(self.make_field('Default', cv_default))
        else:
            fl.append(self.make_field('Default', sphinx.addnodes.literal_emphasis(text='*NONE*')))
        if ('reloadable' in self.options):
            fl.append(self.make_field('Reloadable', 'Yes'))
        if ('deprecated' in self.options):
            fl.append(self.make_field('Deprecated', 'Yes'))

        # Get any contained content
        nn = nodes.compound();
        self.state.nested_parse(self.content, self.content_offset, nn)

        return [ anchor, title, fl, nn ]

class TSConfVarRef(XRefRole):
    def process_link(self, env, ref_node, explicit_title_p, title, target):
        return title, target


class TrafficServerDomain(Domain):
    """
    Apache Traffic Server Documentation.
    """

    name = 'ts'
    label = 'Traffic Server'
    data_version = 2

    object_types = {
        'cv': ObjType(l_('configuration variable'), 'cv')
    }

    directives = {
        'cv' : TSConfVar
    }

    roles = {
        'cv' : TSConfVarRef()
    }

    initial_data = {
        'cv' : {} # full name -> docname
    }

    dangling_warnings = {
        'cv' : "No definition found for configuration variable '%(target)s'"
    }

    def clear_doc(self, docname):
        cv_list = self.data['cv']
        for var, doc in cv_list.items():
            if doc == docname:
                del cv_list[var]

    def find_doc(self, key, obj_type):
        zret = None

        if obj_type == 'cv' :
            obj_list = self.data['cv']
        else:
            obj_list = None

        if obj_list and key in obj_list:
            zret = obj_list[key]

        return zret

    def resolve_xref(self, env, src_doc, builder, obj_type, target, node, cont_node):
        dst_doc = self.find_doc(target, obj_type)
        if (dst_doc):
            return sphinx.util.nodes.make_refnode(builder, src_doc, dst_doc, nodes.make_id(target), cont_node, 'records.config')

    def get_objects(self):
        for var, doc in self.data['cv'].iteritems():
            yield var, var, 'cv', doc, var, 1

def setup(app):
    app.add_crossref_type('configfile', 'file',
                        objname='Configuration file',
                        indextemplate='pair: %s; Configuration files')
    app.add_domain(TrafficServerDomain)