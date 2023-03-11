#!/usr/bin/python3
import os
import re
import sys
from xml.etree import ElementTree


# Elements that shall be generated
TO_GENERATE = {
        'Shader Channel Select',
        'Clear Color',

        'INTERFACE_DESCRIPTOR_DATA',
        'BINDING_TABLE_STATE',
        'RENDER_SURFACE_STATE',

        'PIPELINE_SELECT',
        'MI_LOAD_REGISTER_IMM',
        'PIPE_CONTROL',
        'MEDIA_VFE_STATE',
        'MI_BATCH_BUFFER_START',
        'MI_BATCH_BUFFER_END',
        'MI_NOOP',
        'STATE_BASE_ADDRESS',
        'MEDIA_STATE_FLUSH',
        'MEDIA_INTERFACE_DESCRIPTOR_LOAD',
        'GPGPU_WALKER',

        'L3CNTLREG',
        'CS_CHICKEN1'
}


def error(msg):
    print(msg, file=sys.stderr)
    sys.exit(1)

def split_filename(name):
    return [e for e in re.split(r'[_.-]', name) if e]


def empty_ctx():
    return {
            'enums': set()
    }


def escape_c_symbol(s):
    s = s.replace(' ', '').replace('-', '').replace('/', '_')
    if re.match(r'^[0-9]', s):
        return '_' + s
    else:
        return s

def convert_field_name(n):
    n = n.lower().replace(' ', '_').replace('-', '_')\
            .replace('[', '').replace(']', '')

    n = escape_c_symbol(n)
    return n

def convert_class_name(n):
    n = n.split('_')
    n = ''.join(c.capitalize() for c in n)
    return escape_c_symbol(n)

def convert_enum_name(n):
    return escape_c_symbol(n.replace(' ', ''))

def type_to_c_type(ctx, t, width):
    ct = None

    if t in ctx['enums']:
        ct = 'enum %s' % convert_enum_name(t)

    if ct is None:
        ct = {
                'int': 'int32_t',
                'uint': 'uint32_t',
                'u4.1': 'uint32_t',
                'u4.8': 'uint32_t',
                'address': 'uint64_t',
                'offset': 'uint64_t',
                'bool': 'bool',
                'float': 'float'
        }.get(t)

        if ct == 'int32_t' and width > 32:
            ct = 'int64_t'

        if ct == 'uint32_t' and width > 32:
            ct = 'uint64_t'

        if ct == 'float' and width > 32:
            ct = 'double'

    if ct is None:
        error("Invalid type `%s'" % t)

    return ct

def max_int_type(ctx, t):
    if t in ctx['enums']:
        return 'uint32_t'

    mt = {
            'int': 'int64_t',
            'uint': 'uint64_t',
            'u4.1': 'uint32_t',
            'u4.8': 'uint32_t',
            'address': 'uint64_t',
            'offset': 'uint64_t',
            'bool': 'uint32_t',
            'float': 'uint32_t'
    }.get(t)

    if mt is None:
        error("No max_int_type defined for `%s'" % t)

    return mt

def generate_conversion_to(ctx, t, width, expr):
    output = ''

    expr = '(%s)' % expr

    if t in ctx['enums']:
        output = expr;
    elif t == 'bool':
        output = '%s > 0 ? true : false' % expr
    elif t in ('int', 'uint', 'address', 'offset'):
        output = '(%s) %s' % (type_to_c_type(ctx, t, width), expr)
    else:
        error("No typecast to `%s' defined" % t)

    return '(%s)' % output

def generate_conversion_to_packed(ctx, expr, t):
    if t in ctx['enums']:
        return 'static_cast<int>(%s)' % expr
    elif t == 'int':
        return expr
    elif t == 'uint':
        return expr
    elif t in ('u4.1', 'u4.8'):
        return expr
    elif t == 'bool':
        return '(%s) ? 1 : 0' % expr
    elif t == 'address':
        return expr
    elif t == 'offset':
        return expr
    elif t == 'float':
        return 'custom_bytewise_cast<uint32_t>(%s)' % expr
    else:
        error("No conversion to packed defined for type `%s'" % t)

def generate_conversion_from_packed(ctx, expr, t):
    if t in ctx['enums']:
        return 'static_cast<%s>(%s)' % (convert_enum_name(t), expr)
    elif t == 'int':
        return expr
    elif t == 'uint':
        return expr
    elif t in ('u4.1', 'u4.8'):
        return expr
    elif t == 'bool':
        return '(%s) > 0 ? true : false' % expr
    elif t == 'address':
        return expr
    elif t == 'offset':
        return expr
    elif t == 'float':
        return 'custom_bytewise_cast<float>(%s)' % expr
    else:
        error("No conversion from packed defined for type `%s'" % t)


def generate_enum(ctx, elem):
    text = ''
    ctx['enums'].add(elem.attrib['name'])
    enum_name = convert_enum_name(elem.attrib['name'])
    text += "enum %s\n{\n" % enum_name

    last = len(elem) - 1
    for i,val in enumerate(elem):
        if val.tag != 'value':
            error("Invalid enum XML tag: `%s'" % val.tag)

        delim = '' if i == last else ','
        text += "\t%s_%s = %s%s\n" % (escape_c_symbol(elem.attrib.get('prefix', '')),
                val.attrib['name'], val.attrib['value'], delim)

    text += "};\n\n"
    return text


def generate_struct_field(ctx, cnt_dwords, field, offset, group_id=None):
    if field.tag != 'field':
        error("Invalid struct field XML tag: `%s'" % field.tag)

    output = ''

    start = offset + int(field.attrib['start'])
    end = offset + int(field.attrib['end'])
    if start > end:
        error("start > end")

    size = start - end + 1
    type_ = field.attrib['type']
    fn_name = convert_field_name(field.attrib['name'])

    if group_id is not None:
        name += '_%d' % group_id

    c_type = type_to_c_type(ctx, type_, size)

    # Literals
    literals = ''

    for c in field:
        if c.tag != 'value':
            error("Field %s has unknown XML-child with tag `%s'." % (
                field.attrib['name'], c.tag))

        literals += '\tstatic constexpr %s %s = %d;\n' % (
                c_type,
                escape_c_symbol('%s_%s' % (field.attrib['name'], c.attrib['name'])),
                int(c.attrib['value']))

    if literals:
        output = ('%s\n' % literals) + output

    # Accessor-functions
    get_code = '\t\t%s v{};\n' % max_int_type(ctx, type_)
    set_code = ''

    for i in range(cnt_dwords):
        dw_start = i * 32
        dw_end = dw_start + 31

        if start > dw_end or end < dw_start:
            continue

        sw = start - dw_start
        shift = '<<' if sw >= 0 else '>>'
        sw = -sw if sw < 0 else sw

        mask_start = max(start, dw_start) - dw_start
        mask_end = min(end, dw_end) - dw_start
        mask = ((1 << (mask_end - mask_start + 1)) - 1) << mask_start
        inv_mask = 0xffffffff ^ mask

        val = generate_conversion_to_packed(ctx, 'v', type_)
        expr = '((unsigned long long) (%s) %s %dULL) & 0x%08xULL' % (val, shift, sw, mask)
        set_code += '\t\tdata[%d] = (data[%d] & 0x%08x) | (%s);\n' % \
                (i, i, inv_mask, expr)

        get_sw = dw_start - start
        get_shift = '<<' if get_sw >= 0 else '>>'
        get_sw = -get_sw if get_sw < 0 else get_sw

        get_mask_start = max(start, dw_start) - start
        get_mask_end = min(end, dw_end) - start
        get_mask = ((1 << (get_mask_end - get_mask_start + 1)) - 1) << get_mask_start

        get_code += '\t\tv |= ((unsigned long long) data[%d] %s %dULL) & 0x%xULL;\n' % \
                (i, get_shift, get_sw, get_mask)

    get_code += '\t\treturn %s;\n' % generate_conversion_from_packed(ctx, 'v', type_)

    output += '\t%s get_%s() const\n\t{\n%s\t};\n\n' % (c_type, fn_name, get_code)
    output += '\tvoid set_%s(%s v)\n\t{\n%s\t};\n\n' % (fn_name, c_type, set_code)

    return output


def generate_struct(ctx, elem):
    cnt_dwords = int(elem.attrib['length'])

    struct_name = elem.attrib['name']

    text = ''
    text += 'struct %s\n{\n' % elem.attrib['name']
    text += '\tuint32_t data[%d] = { 0 };\n\n' % cnt_dwords
    text += '\tstatic constexpr size_t cnt_bytes = %d;\n' % (cnt_dwords * 4)

    for field in elem:
        if field.tag == 'field':
            if max(int(field.attrib['start']), int(field.attrib['end'])) > cnt_dwords * 32:
                error("Struct field %s::%s lays out of the register's bound." %
                        (elem.attrib['name'], field.attrib['name']))

            text += generate_struct_field(ctx, cnt_dwords, field, 0)

        else:
            error("Struct %s has field with invalid XML tag `%s'." %
                    (elem.attrib['name'], field.tag))

    text += '} __attribute__((packed));\n'
    text += 'static_assert(sizeof(%s) == %d);\n\n' % (struct_name, cnt_dwords * 4);

    return text


def generate_instruction(ctx, elem):
    cnt_dwords = int(elem.attrib['length'])

    cls_name = 'Cmd' + convert_class_name(elem.attrib['name'])

    text = ''
    text += 'class %s : public I915RingCmd\n{\n' % cls_name

    # Fields
    protected_fields = ''
    public_fields = ''

    fields_to_gen = []

    for field in elem:
        if field.tag == 'field':
            name = convert_field_name(field.attrib['name'])
            type_ = field.attrib['type']
            start = int(field.attrib['start'])
            end = int(field.attrib['end'])
            size = end - start + 1
            ctype = type_to_c_type(ctx, type_, size)

            fields_to_gen.append((start, end, name, type_))

            default = field.attrib.get('default')
            if default is not None:
                init = ' = %s' % int(default)
            else:
                init = '{}'

            field_text = '\t%s %s%s;\n' % (ctype, name, init)

            prefix = field.attrib.get('prefix', '')
            have_vals = False
            for val in field:
                if val.tag != 'value':
                    error("Field %s::%s has invalid child XML element `%s'" %
                            elem.attrib['name'], field.attrib['name'], val.tag)

                have_vals = True

                val_name = val.attrib['name']
                if prefix:
                    val_name = prefix + '_' + val_name

                field_text += "\tstatic constexpr %s %s = %s;\n" % (
                        ctype,
                        escape_c_symbol(val_name),
                        int(val.attrib['value']))

            if have_vals:
                field_text += '\n'

            if default is None:
                public_fields += field_text
            else:
                protected_fields += field_text

        elif field.tag == 'group':
            grp_count = int(field.attrib['count'])
            grp_start = int(field.attrib['start'])
            grp_size = int(field.attrib['size'])

            if grp_count != 0:
                error("<group> with count != 0 is not supported for instructions yet")

        else:
            error("Field of %s has invalid xml tag `%s'" %
                    (elem.attrib['name'], field.tag))


    if protected_fields:
        text += 'protected:\n' + protected_fields + '\n'

    text += 'public:\n' + public_fields

    # Methods
    text += '\n\n\t~%s()\n\t{\n\t}\n\n' % cls_name

    text += '\tsize_t bin_size() const override\n\t{\n\t\t' \
            'return %d;\n\t}\n' % (cnt_dwords * 4)

    # Assemble command
    text += '\n\tsize_t bin_write(char* _dst) const override\n\t{\n'
    text += '\t\tauto dst = reinterpret_cast<uint32_t*>(_dst);\n'

    for i in range(cnt_dwords):
        text += '\n\t\tdst[%d] = 0ULL' % i
        bit_start = i * 32
        bit_end = bit_start + 31

        for start, end, name, type_ in fields_to_gen:
            if start > bit_end or end < bit_start:
                continue

            size = end - start + 1
            sw = start - bit_start
            shift = '<<' if sw >= 0 else '>>'
            sw = -sw if sw < 0 else sw

            mask_start = max(start, bit_start) - bit_start
            mask_end = min(end, bit_end) - bit_start
            mask = ((1 << (mask_end - mask_start + 1)) - 1) << mask_start

            val = generate_conversion_to_packed(ctx, name, type_)
            expr = '((unsigned long long) (%s) %s %dULL) & 0x%08xULL' % (val, shift, sw, mask)
            text += ' | \n\t\t\t\t(%s)' % expr

        text += ';\n'

    text += '\n\t\treturn %d;\n\t}\n' % (cnt_dwords * 4)

    text += '};\n\n'
    return text


def generate_register(ctx, elem):
    cnt_dwords = int(elem.attrib['length'])
    num = int(elem.attrib['num'], base=0)

    struct_name = 'REG_%s' % elem.attrib['name']

    text = ''
    text += 'struct %s\n{\n' % struct_name
    text += '\tuint32_t data[%d] = { 0 };\n' % cnt_dwords
    text += '\tstatic constexpr size_t cnt_dwords = %d;\n' % cnt_dwords
    text += '\tstatic constexpr uint32_t address = 0x%x;\n\n' % num

    for field in elem:
        if field.tag == 'field':
            if max(int(field.attrib['start']), int(field.attrib['end'])) > cnt_dwords * 32:
                error("Register field %s::%s lays out of the register's bound." %
                        (elem.attrib['name'], field.attrib['name']))

            text += generate_struct_field(ctx, cnt_dwords, field, 0)

        else:
            error("Register %s has field with invalid XML tag `%s'." %
                    (elem.attrib['name'], field.tag))

    text += '} __attribute__((packed));\n'
    text += 'static_assert(sizeof(%s) == %d);\n\n' % (struct_name, cnt_dwords * 4);

    return text


def generate_hdr(xml_text, file_name):
    hdr_text = ''
    root = ElementTree.fromstring(xml_text)
    if root.tag != 'genxml':
        error("Invalid XML root element")

    gen_ver = root.attrib['gen']
    gen_str = 'Gen' + gen_ver.replace('.', '_')

    hdr_text += \
"""/** GEN architecture specific low-level hardware interface header, generated
 * from an xml-file, which in turn was copied from the MESA project. These
 * xml-files are located in the directory /src/intel/genxml of the MESA source
 * tree, e.g. available here: https://gitlab.freedesktop.org/mesa/mesa. The
 * xml-file has no copyright notice itself, but the header files generated
 * within the MESA protect get the following license header:
 *
 *
 * Copyright (C) 2016 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

"""

    # Add include guard
    include_guard_name = ('__' + '_'.join(split_filename(file_name))).upper()
    hdr_text += "#ifndef %s\n" % include_guard_name
    hdr_text += "#define %s\n" % include_guard_name

    hdr_text += "\n#include <stdint.h>\n"
    hdr_text += "\n#include <cstring>\n"
    hdr_text += "\nstatic_assert(sizeof(float) == 4);\n\n"

    hdr_text += """/* Like C++20's std::bit_cast but for older compilers */
template<typename Y, typename X>
inline Y custom_bytewise_cast(X x)
{
\tstatic_assert(sizeof(X) == sizeof(Y));
\tY y;
\tmemcpy(&y, &x, sizeof(X));
\treturn y;
}
"""

    # Add namespace
    hdr_text += "\nnamespace OCL::HWInt::%s {\n\n" % gen_str

    ctx = empty_ctx()

    for elem in root:
        if elem.attrib.get('name') not in TO_GENERATE:
            continue

        if elem.tag == 'enum':
            hdr_text += generate_enum(ctx, elem)

        elif elem.tag == 'struct':
            hdr_text += generate_struct(ctx, elem)

        elif elem.tag == 'instruction':
            hdr_text += generate_instruction(ctx, elem)

        elif elem.tag == 'register':
            hdr_text += generate_register(ctx, elem)

        else:
            error("Invalid XML tag `%s'" % elem.tag)

    hdr_text += "}\n\n"

    # Add include guard trailer
    hdr_text += "#endif /* %s */\n" % include_guard_name
    return hdr_text


def main():
    if len(sys.argv) != 3:
        error("Usage: %s <xml file> <output header file>" % sys.argv[0])

    src_file = sys.argv[1]
    dst_file = sys.argv[2]
    dst_name = os.path.basename(dst_file)

    with open(src_file, 'r', encoding='utf8') as f_src:
        with open(dst_file, 'w', encoding='utf8') as f_dst:
            f_dst.write(generate_hdr(f_src.read(), dst_name))


if __name__ == '__main__':
    main()
    sys.exit(0)
