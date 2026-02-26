#!/usr/bin/env python3
import re
import sys


def find_matching_angle(text, start):
    """Return the index of the '>' matching the '<' at *start*, or -1.

    Skips over string literals so commas / brackets inside them are ignored.
    """
    depth = 0
    i = start
    while i < len(text):
        c = text[i]
        if c in ('"', "'"):
            # Skip string literal
            quote = c
            i += 1
            while i < len(text):
                if text[i] == '\\':
                    i += 2
                    continue
                if text[i] == quote:
                    break
                i += 1
        elif c == '<':
            depth += 1
        elif c == '>':
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def split_top_level_args(args_str):
    """Split *args_str* by top-level commas, respecting angle brackets, parens,
    braces and string literals.
    """
    args = []
    depth = 0
    current = []
    i = 0
    while i < len(args_str):
        c = args_str[i]
        if c in ('"', "'"):
            # Consume the entire string literal verbatim
            quote = c
            current.append(c)
            i += 1
            while i < len(args_str):
                ch = args_str[i]
                current.append(ch)
                if ch == '\\':
                    i += 1
                    if i < len(args_str):
                        current.append(args_str[i])
                elif ch == quote:
                    break
                i += 1
        elif c in '<({':
            depth += 1
            current.append(c)
        elif c in '>)}':
            depth -= 1
            current.append(c)
        elif c == ',' and depth == 0:
            args.append(''.join(current).strip())
            current = []
            i += 1
            continue
        else:
            current.append(c)
        i += 1
    if current:
        args.append(''.join(current).strip())
    return args


def substitute_templates(text, substitutions):
    """Single pass over *text* replacing template instantiations.

    *substitutions* maps ``template_name`` → ``handler(args) -> str | None``.
    When a handler returns ``None`` the original text is left unchanged.

    Longer pattern names are tried first so that e.g. ``std::integral_constant``
    is not shadowed by a hypothetical ``std::integral``.
    """
    # Sort by descending length to avoid prefix collisions
    ordered = sorted(substitutions.items(), key=lambda kv: -len(kv[0]))

    result = []
    i = 0
    while i < len(text):
        matched = False
        for pattern, handler in ordered:
            end_of_name = i + len(pattern)
            if (text[i:end_of_name] == pattern
                    and end_of_name < len(text)
                    and text[end_of_name] == '<'):
                bracket_end = find_matching_angle(text, end_of_name)
                if bracket_end != -1:
                    args_str = text[end_of_name + 1:bracket_end]
                    args = split_top_level_args(args_str)
                    replacement = handler(args)
                    if replacement is not None:
                        result.append(replacement)
                        i = bracket_end + 1
                        matched = True
                        break
        if not matched:
            result.append(text[i])
            i += 1
    return ''.join(result)



def count_template_params(text, start_pos):
    """Count the number of template parameters starting from a '<' character."""
    depth = 0
    param_count = 0
    i = start_pos

    while i < len(text):
        char = text[i]

        if char == '<':
            if depth == 0:
                param_count = 1  # at least one parameter
            depth += 1
        elif char == '>':
            depth -= 1
            if depth == 0:
                segment = text[start_pos:i + 1]
                if segment.strip() == '<>':
                    return 0
                return param_count
        elif char == ',' and depth == 1:
            param_count += 1

        i += 1

    return param_count


def format_template_error(text, min_params=5):
    """Format C++ template error messages with proper indentation.

    Each template instantiation is evaluated independently. Only templates
    with >= min_params parameters are formatted with indentation.
    """
    lines = []
    current_line = ""
    indent_level = 0
    indent_stack = []  # Track which levels should be indented

    i = 0
    while i < len(text):
        char = text[i]

        if char == '<':
            if i > 0 and (text[i - 1].isalnum() or text[i - 1] in '_>'):
                param_count = count_template_params(text, i)
                should_indent = param_count >= min_params

                current_line += char

                if should_indent:
                    lines.append("  " * indent_level + current_line)
                    current_line = ""
                    indent_level += 1
                    indent_stack.append(True)
                else:
                    indent_stack.append(False)
            else:
                current_line += char
        elif char == '>':
            if indent_stack and indent_stack[-1]:
                if current_line.strip():
                    lines.append("  " * indent_level + current_line)
                    current_line = ""
                indent_level = max(0, indent_level - 1)
                lines.append("  " * indent_level + ">")
                indent_stack.pop()
            else:
                current_line += char
                if indent_stack:
                    indent_stack.pop()
        elif char == ',':
            if indent_stack and indent_stack[-1]:
                current_line += char
                lines.append("  " * indent_level + current_line)
                current_line = ""
            else:
                current_line += char
        elif char == '\n':
            if current_line.strip():
                lines.append("  " * indent_level + current_line)
                current_line = ""
            lines.append("")
        else:
            current_line += char

        i += 1

    if current_line.strip():
        lines.append("  " * indent_level + current_line)

    return '\n'.join(lines)


def clean_template_names(text):
    """Clean up common verbose template patterns using bracket-aware parsing."""

    # Simple string replacements first
    text = text.replace('18446744073709551615', 'SIZE_T_MAX')

    # --- Handler definitions ---

    def sub_conststr(args):
        # ConstStr<ConstStrContainer<N>{"content"}>
        if len(args) == 1:
            m = re.match(r'ConstStrContainer<\d+>\{"([^"]*)"\}$', args[0].strip())
            if m:
                return f'Str<"{m.group(1)}">'
        return None

    def sub_integral_constant(args):
        # std::integral_constant<Type, Value>
        if len(args) == 2:
            return f'IC<{args[1].strip()}>'
        return None

    def sub_pair(args):
        if len(args) == 2:
            return f'pair<{args[0]}, {args[1]}>'
        return None

    def sub_tuple(args):
        return f'tuple<{", ".join(args)}>'

    substitutions = {
        'std::integral_constant': sub_integral_constant,
        'ConstStr':               sub_conststr,
        'std::pair':              sub_pair,
        'std::tuple':             sub_tuple,
    }

    # Iterate until the text stabilises (handles nested substitutions)
    prev = None
    while text != prev:
        prev = text
        text = substitute_templates(text, substitutions)

    return text


def main():
    min_params = 5
    args = sys.argv[1:]
    input_file = None

    i = 0
    while i < len(args):
        if args[i] in ['-k', '--min-params']:
            if i + 1 < len(args):
                min_params = int(args[i + 1])
                i += 2
            else:
                print("Error: -k/--min-params requires a value", file=sys.stderr)
                sys.exit(1)
        else:
            input_file = args[i]
            i += 1

    if input_file:
        with open(input_file, 'r') as f:
            content = f.read()
    else:
        content = sys.stdin.read()

    content = re.sub(r'\s+', ' ', content).strip()
    cleaned = clean_template_names(content)
    formatted = format_template_error(cleaned, min_params=min_params)
    print(formatted)


if __name__ == "__main__":
    main()
