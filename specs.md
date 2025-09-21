# Papr

.papr is an open file format designed to store arrays of key-value pairs in an intuitive and human-readable fashion. This is done by aligning the tokens in each line relative to the depth of the tokens in previous lines using leading spaces.

## Specs

### Basic Rules
- A key-value pair relationship between two tokens can be established using a colon symbol
- Key-value pair relationships can be created between many children and one parent by aligning the leading colon relative to the closest establishing colon
- More data can be appended to a child object by aligning a new line that doesn't have a leading colon relative to the closest leading colon
- Single-line comments can be added using the hashtag symbol

The rules feel super complicated, but the format makes so much intuitive sense. So, I'll try my best to explain them with examples in the following sections.

### Token Rules
- A token is delimited by a colon symbol or a new line
- All leading and trailing spaces in a token are trimmed off
- A token starting with a double quote symbol can include special symbols such as the colon, new line, hashtag, and any leading and trailing spaces, as long as there's another double quote to wrap the contents and inform where the token ends
  - Double quotes included in the middle of a token are treated as just another character
  - To include a double quote inside a token wrapped with double quotes, prepend it with a forward slash
  - If the token wrapped with a double quote contains a new line, the following line must be padded with spaces such that the first character in the new line is aligned to be 1 character after the double quote that started this token.

```
campaign: type:        pathfinder
          title:       "Dimension 20: A starstruck odyssey"
          description: "This campaign follows the story of 6
                        intrepid heroes in the deep space..."
```

### Establishing Colon
A simple key-value pair relationship between tokens can be established using the colon symbol.

```
name: John
age:  42
```

Here, for the key 'name', the associated value is the token 'John', and for the key 'age', the associated value is the token '42'. These types of colons that establish a brand new relationship between two tokens and increase the depth of the tree will be referred to as establishing colons.

### Arrays
Key-value pair relationships can be created between many children and one parent by aligning the leading colon relative to the closest establishing colon. The colon in a new line whose first character is that colon (ignoring the aligning spaces) is referred to as the leading colon. 

In the example below, the tokens 'spring', 'summer', 'fall', and 'winter' are all an array of values associated with the parent token 'seasons'. This allows us to create lists. 

```
seasons: spring
       : summer
       : fall
       : winter
```

The positioning of the leading colon doesn't have to be exact and will fall back to the closest establishing colon to the left of it. This lets us neatly align content when the tree structure is a lot more complicated with a lot of nested variables. 

The example below looks wonky, yet it is a valid representation of the same relationship as the example above. A more natural way of thinking about this is that the tokens after a leading colon will create a relationship with the immediate parent to the left of it.
```
seasons: spring
         : summer
        : fall
          : winter
```

### Nesting
When a new line doesn't have a leading colon, more data can be appended to an existing child by aligning the contents relative to the closest leading colon.

In the example below, the first element of the 'members' key has the object 'name' and 'age' with their respective values. Both 'name' and 'age' are part of the same object.

```
members: name: John Doe
         age: 42
```

Nested objects can also be part of lists, like the example below. A useful way of going about this would be that a parent token always has a one-to-many relationship, and an establishing token just adds the first element to the list.

```
members: name: John Doe
         age: 42
       : name: Jane Doe
         age: 39
```

Things can be deeply nested, like, in the example below, where the 'name' token, as the parent, has 'first', 'last', and 'middle' tokens as children.

```
members: name: first: John
               last: Doe
         age: 42
       : name: first: Jane
               middle: Orchard
               last: Doe
       : age: 39
```

An interesting property happens when we try to append additional data to a leaf node. Nesting cannot happen in a leaf node because they don't necessarily have the establishing colon to create the first parent-child relationship. In that case, the entire line is considered to be a string token and is appended to the previous line(s). This makes paragraphs of data more readable.

```
artists: name: The Midnight
         description: The Midnight consists of Tyler Lyle (a songwriter from 
                      Deep South) and Tim McEwan (a producer from Denmark).

```

### Comments
Comments are useful in providing additional context to the data stored in a .papr file. A single-line comment can be added using a hashtag symbol, which is terminated by a new line.

Here are some examples:
```
# the level names starting with d_ are dev-only sandboxes
levels: dragon road
      : sparkles lane # todo: fix typo in menu
     #: d_loopTest
     #: d_tmpLvl0
      : tutorial drive
```

I debated a lot on whether to include comments as a core feature of .papr or not because, without comments, the rules are much simpler. There could be just one reserved character (the colon symbol) and maybe the quotes. While it's non-information for a parser and is completely ignored by it, the goal of the .papr format is to prioritize readability. So in the end, I decided to add comments as they help a lot with enhancing a reader's experience.
