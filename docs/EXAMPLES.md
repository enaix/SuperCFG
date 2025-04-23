# Examples

## Calculator Grammar

Here's an example for a simple calculator grammar that supports basic arithmetic operations:

```cpp
// Define basic number components
constexpr auto digit = NTerm(cs<"digit">());
constexpr auto d_digit = Define(digit, Repeat(Alter(Term(cs<"1">()), Term(cs<"2">()), /* ... */)));

constexpr auto number = NTerm(cs<"number">());
constexpr auto d_number = Define(number, Repeat(digit));

// Define arithmetic operations
constexpr auto add = NTerm(cs<"add">());
constexpr auto sub = NTerm(cs<"sub">());
constexpr auto mul = NTerm(cs<"mul">());
constexpr auto div = NTerm(cs<"div">());
constexpr auto op = NTerm(cs<"op">());
constexpr auto arithmetic = NTerm(cs<"arithmetic">());
constexpr auto group = NTerm(cs<"group">());

// No operator order defined: grammar is ambiguous
constexpr auto d_add = Define(add, Concat(op, Term(cs<"+">()), op));
constexpr auto d_sub = Define(sub, Concat(op, Term(cs<"-">()), op));
constexpr auto d_mul = Define(mul, Concat(op, Term(cs<"*">()), op));
constexpr auto d_div = Define(div, Concat(op, Term(cs<"/">()), op));

// Define grouping and operator rules
constexpr auto d_group = Define(group, Concat(Term(cs<"(">()), op, Term(cs<")">())));
constexpr auto d_arithmetic = Define(arithmetic, Alter(add, sub, mul, div));
constexpr auto d_op = Define(op, Alter(number, arithmetic, group));

// Combine all rules
constexpr auto ruleset = RulesDef(d_digit, d_number, d_add, d_sub, d_mul, d_div,
                                 d_arithmetic, d_op, d_group);
```

## JSON Grammar

Here's a complete JSON grammar example that supports objects, arrays, strings, numbers, booleans, and null values:

```cpp
// Define character set for strings
constexpr char s[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ _-.!";

// Define basic components
constexpr auto character = NTerm(cs<"char">());
constexpr auto digit = NTerm(cs<"digit">());
constexpr auto number = NTerm(cs<"number">());
constexpr auto boolean = NTerm(cs<"bool">());
constexpr auto json = NTerm(cs<"json">());
constexpr auto object = NTerm(cs<"object">());
constexpr auto null = NTerm(cs<"null">());
constexpr auto string = NTerm(cs<"string">());
constexpr auto array = NTerm(cs<"array">());
constexpr auto member = NTerm(cs<"member">());

// Define character set using build_range helper
constexpr auto d_character = Define(character, Repeat(build_range(cs<s>(), 
    [](const auto&... str){ return Alter(Term(str)...); }, 
    std::make_index_sequence<sizeof(s)-1>{})));

// Define number components
constexpr auto d_digit = Define(digit, Repeat(Alter(
    Term(cs<"1">()), Term(cs<"2">()), Term(cs<"3">()),
    Term(cs<"4">()), Term(cs<"5">()), Term(cs<"6">()),
    Term(cs<"7">()), Term(cs<"8">()), Term(cs<"9">()),
    Term(cs<"0">())
)));
constexpr auto d_number = Define(number, Repeat(digit));

// Define JSON value types
constexpr auto d_boolean = Define(boolean, Alter(Term(cs<"true">()), Term(cs<"false">())));
constexpr auto d_null = Define(null, Term(cs<"null">()));
constexpr auto d_string = Define(string, Concat(Term(cs<"\"">()), Repeat(character), Term(cs<"\"">())));

// Define array and object structures
constexpr auto d_array = Define(array, Concat(
    Term(cs<"[">()), 
    json, 
    Repeat(Concat(Term(cs<",">()), json)), 
    Term(cs<"]">())
));

constexpr auto d_member = Define(member, Concat(
    json, 
    Term(cs<":">()), 
    json
));

constexpr auto d_object = Define(object, Concat(
    Term(cs<"{">()), 
    member, 
    Repeat(Concat(Term(cs<",">()), member)), 
    Term(cs<"}">())
));

// Define the root JSON rule
constexpr auto d_json = Define(json, Alter(
    array, boolean, null, number, object, string
));

// Combine all rules
constexpr auto ruleset = RulesDef(
    d_character, d_digit, d_number, d_boolean, 
    d_null, d_string, d_array, d_member, 
    d_object, d_json
);
```

This JSON grammar supports:
- Numbers (e.g., `42`, `123`)
- Strings (e.g., `"hello"`, `"world"`)
- Arrays (e.g., `[1,2,3]`, `["a","b","c"]`)
- Objects (e.g., `{"key":"value"}`, `{"a":1,"b":2}`)
- Nested structures (e.g., `{"a":[1,2,3],"b":{"c":"d"}}`)

JSON parser without escape characters and whitespaces can be found in `examples/json.cpp`. Example strings: `42`, `"hello"`, `[1,2,3]`, `[1,["abc",2],["d","e","f"]]`, `{"a":123,"b":456}`, `{"a":[1,2,"asdf"],"b":["q","w","e"]}`, `{"a":{"b":42,"c":"abc"},"qwerty":{1:"uiop",42:10}}`
