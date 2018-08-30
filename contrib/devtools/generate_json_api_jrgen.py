#!/usr/bin/env python3

import re
import subprocess as sp
import sys
import json
import urllib.request as req
import jsonschema


re_full = re.compile(r'(?P<name>^.*?$)(?P<desc>.*?)(^Argument.*?$(?P<args>.*?))?(^Result[^\n,]*?:\s*$(?P<resl>.*?))?(^Exampl.*?$(?P<exmp>.*))?', re.DOTALL | re.MULTILINE)
re_argline = re.compile(r'^("?)(?P<name>\w.*?)\1(\s*:.+?,?\s*)?\s+\((?P<type>.*?)\)\s*(?P<desc>.*?)\s*$', re.DOTALL)


def get_obj_from_dirty_text(full_object: str):
    lines = full_object.splitlines()
    lefts = []
    i = 0
    while i < len(lines):
        idx = lines[i].find('(')
        left = lines[i][0:idx].strip() if idx >= 0 else lines[i]
        left = left.rstrip('.')  # handling , ...
        left = left.strip()
        left = left.rstrip(',')
        lefts.append(left)
        while idx >= 0 and i < len(lines) - 1:
            idx2 = len(re.match(r'^\s*', lines[i + 1]).group())
            if idx2 > idx:
                lines[i] += lines.pop(i + 1)[idx2 - 1:]
            else:
                break
        i += 1

    ret = None
    try:
        property_stack = []
        object_stack = []
        name_stack = []

        last_name = None
        for i in range(0, len(lines)):
            left = lefts[i]
            if not left:
                continue
            line = lines[i].strip()

            arg_parsed = re_argline.fullmatch(line)
            property_refined_type = 'object'
            if arg_parsed is not None:
                property_name, property_type, property_desc = arg_parsed.group('name', 'type', 'desc')
                property_refined_type, property_required, property_child = get_type(property_type, None)

                if property_refined_type is not 'array' and property_refined_type is not 'object':
                    property_stack[-1][property_name] = {
                        'type': property_refined_type,
                        'description': property_desc
                    }
                else:
                    last_name = property_name
            elif len(left) > 1:
                match = re.match(r'^(\[)?"(?P<name>\w.*?)"(\])?.*', left)
                last_name = match.group('name')
                if match.group(1) is not None and match.group(3) is not None:
                    left = '['
                    property_refined_type = 'string'
                    if 'string' not in line:
                        raise NotImplementedError('Not implemented: ' + line)

            if left.endswith('['):
                object_stack.append({'type': 'array', 'items': {'type': property_refined_type}})
                property_stack.append({})
                name_stack.append(last_name)
            elif left.endswith('{'):
                object_stack.append({'type': 'object'})
                property_stack.append({})
                name_stack.append(last_name)
            elif (left.endswith(']') and '[' not in left) or (left.endswith('}') and '{' not in left):
                obj = object_stack.pop()
                prop = property_stack.pop()
                name = name_stack.pop()
                if len(prop) > 0:
                    if 'items' in obj:
                        obj['items']['properties'] = prop
                    else:
                        obj['properties'] = prop
                if len(property_stack) > 0:
                    if 'items' in object_stack[-1]:
                        object_stack[-1]['items']['type'] = obj['type']
                        if len(prop) > 0:
                            object_stack[-1]['items']['properties'] = prop
                    else:
                        if name is None:
                            raise RuntimeError('Not expected')
                        property_stack[-1][name] = obj
                else:
                    ret = obj
            if ret is not None:
                if i + 1 < len(lines) - 1:
                    print('Ignoring this data (below the parsed object): ' + "\n".join(lines[i+1:]), file=sys.stderr)
                return ret
    except Exception as e:
        print('Exception: ' + str(e), file=sys.stderr)
        print('Unable to cope with: ' + '\n'.join(lines), file=sys.stderr)
    return None


def get_type(arg_type: str, full_line: str):
    if arg_type is None:
        return 'string', True, None

    required = 'required' in arg_type or 'optional' not in arg_type

    arg_type = arg_type.lower()
    if 'array' in arg_type:
        return 'array', required, None
    if 'numeric' in arg_type:
        return 'number', required, None
    if 'bool' in arg_type:
        return 'boolean', required, None
    if 'string' in arg_type:
        return 'string', required, None
    if 'object' in arg_type:
        properties = get_obj_from_dirty_text(full_line) if full_line is not None else None
        return 'object', required, properties

    print('Unable to derive type from: ' + arg_type, file=sys.stderr)
    return None, False, None


def get_default(arg_refined_type: str, arg_type: str):
    if 'default=' in arg_type:
        if 'number' in arg_refined_type:
            return int(re.match('.*default=([^,)]+)', arg_type).group(1))
        if 'string' in arg_refined_type:
            return re.match('.*default=([^,)]+)', arg_type).group(1)
        if 'boolean' in arg_refined_type:
            if 'default=true' in arg_type:
                return True
            if 'default=false' in arg_type:
                return False
            raise NotImplementedError('Not implemented: ' + arg_type)
        if 'array' in arg_type:
            raise NotImplementedError('Not implemented: ' + arg_type)
    return None


def parse_single_argument(line: str):
    if line:
        line = line.strip()
    if not line or line.startswith('None'):
        return None, None, False

    arg_parsed = re_argline.fullmatch(line)
    if arg_parsed is None:
        if line.startswith('{') or line.startswith('['):
            return get_obj_from_dirty_text(line), None, True
        else:
            print("Unparsable argument: " + line, file=sys.stderr)
        descriptor = {
            'type': 'array' if line.startswith('[') else 'object',
            'description': line,
        }
        return descriptor, None, True

    arg_name, arg_type, arg_desc = arg_parsed.group('name', 'type', 'desc')
    if not arg_type:
        raise NotImplementedError('Not implemented: ' + arg_type)
    arg_refined_type, arg_required, arg_properties = get_type(arg_type, arg_desc)

    if arg_properties is not None:
        return arg_properties, arg_name, arg_required

    arg_refined_default = get_default(arg_refined_type, arg_type)
    arg_desc = re.sub('\s+', ' ', arg_desc.strip()) \
        if arg_desc and arg_refined_type is not 'object' and arg_refined_type is not 'array' \
        else arg_desc.strip() if arg_desc else ''

    descriptor = {
        'type': arg_refined_type,
        'description': arg_desc,
    }
    if arg_refined_default is not None:
        descriptor['default'] = arg_refined_default
    return descriptor, arg_name, arg_required


def parse_params(args: str):
    arguments = {}
    requireds = []
    if args:
        for line in re.split('\s*\d+\.\s+', args, re.DOTALL):
            descriptor, name, required = parse_single_argument(line)
            if descriptor is None:
                continue
            if required:
                requireds.append(name)
            arguments[name] = descriptor
    return arguments, requireds


def get_api(section_name: str, command: str, command_help: str):

    parsed = re_full.fullmatch(command_help)
    if parsed is None:
        raise RuntimeError('Unable to resolve help format for ' + command)

    name, desc, args, resl, exmp = parsed.group('name', 'desc', 'args', 'resl', 'exmp')

    properties, required = parse_params(args)
    result_descriptor, result_name, result_required = parse_single_argument(resl)

    desc = re.sub('\s+', ' ', desc.strip()) if desc else name
    example_array = exmp.splitlines() if exmp else []

    ret = {
        'summary': desc,
        'description': example_array,
        'tags': [section_name],
        'params': {
            'type': 'object',
            'properties': properties,
            'required': required
        },
    }
    if result_descriptor is not None:
        ret['result'] = result_descriptor
    return ret


def write_api():
    if len(sys.argv) < 2:
        print("Missing required argument: <path to CLI tool>", file=sys.stderr)
        sys.exit(1)
    cli_tool = sys.argv[1]
    result = sp.run([cli_tool, "help"], stdout=sp.PIPE, universal_newlines=True)
    commands = result.stdout
    sections = re.split('^==\s*(.*?)\s*==$', commands, flags=re.MULTILINE)
    methods = {}
    for section in sections:
        if not section:
            continue
        lines = section.splitlines()
        if len(lines) == 1:
            section_name = lines[0]
            continue
        for command in sorted(lines[1:]):
            if not command:
                continue
            command = command.split(' ')[0]
            result = sp.run([cli_tool, "help", command], stdout=sp.PIPE, universal_newlines=True)
            methods[command] = get_api(section_name, command, result.stdout)

    version = sp.run([cli_tool, "--version"], stdout=sp.PIPE, universal_newlines=True)
    wrapper = {
        '$schema': 'https://rawgit.com/mzernetsch/jrgen/master/jrgen-spec.schema.json',
        'jrgen': '1.1',
        'jsonrpc': '1.0',  # see https://github.com/bitcoin/bitcoin/pull/12435
        'info': {
            'title': 'lbrycrd RPC API',
            'version': version.stdout.strip(),
            'description': []
        },
        'definitions': {},  # for items used in $ref further down
        'methods': methods,
    }

    schema = req.urlopen(wrapper['$schema']).read().decode('utf-8')
    try:
        jsonschema.validate(wrapper, schema)
    except Exception as e:
        print('From schema validation: ' + str(e), file=sys.stderr)

    print(json.dumps(wrapper, indent=4))


if __name__ == '__main__':
    write_api()