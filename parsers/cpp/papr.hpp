// ----------------------------------------------------------------------------
// Author:      Sliptrixx (Hibnu Hishath)
// Date:        2024-09-22
//
// Description: Parse and serialize data into the .papr file format, an open 
//              file format designed to store readable key-value pairs
// ----------------------------------------------------------------------------

#ifndef PAPR_H
#define PAPR_H

//-----------------------------------------------------------------------------
// Headers
//-----------------------------------------------------------------------------
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
namespace Papr { enum NodeType : uint8_t; }
namespace Papr { struct Node; }
namespace Papr { Node Parse( const std::string& ); }
namespace Papr { std::string Serialize( const Node& ); }
namespace Papr::Internal { enum TokenType : uint8_t; }
namespace Papr::Internal { struct Token; }
namespace Papr::Internal { std::string Trim( const std::string&, size_t ); }
namespace Papr::Internal { std::vector< Token > Tokenize( const std::string& ); }
namespace Papr::Internal { void SerializeRecursive( uint32_t, const Node&, std::string& ); }

//-----------------------------------------------------------------------------
// Papr NodeType Enum
// Summary: There are a few different types of nodes created when a .papr file 
//          is parsed. Those types are represented with this enum.
//-----------------------------------------------------------------------------
enum Papr::NodeType : uint8_t
{
    None,
    Group,
    Key,
    Value,
};

//-----------------------------------------------------------------------------
// Papr Node Struct
// Summary: Stores data representing key-value pairs in a .papr file
//-----------------------------------------------------------------------------
struct Papr::Node
{
public:
    // Invalid node
    static Node INVALID;

    // Builders
    static Node MakeGroup();
    static Node MakeKey( const std::string& key );
    static Node MakeValue( const std::string& value );

    // Constructors and destructors
    Node()  = default;          // Default constructor
    ~Node() = default;          // Default destructor
    Node( const Node& other );  // Copy constructor
    Node( Node&& other );       // Move constructor
    
    Node& operator=( const Node& other );   // Copy assignment operator
    Node& operator=( Node&& other );        // Move assignment operator

    // Iterator
    struct iterator 
    {
        // Note: This is the first time I'm using tags, but apparently it can 
        //       be used to pick the most efficient algorithm for this data 
        //       structure when passed to the standard library
        using iterator_category = std::forward_iterator_tag;
        using value_type = Node;
        using difference_type = std::ptrdiff_t;
        using pointer = Node*;
        using reference = Node&;

        explicit iterator( std::vector< std::unique_ptr< Node > >::iterator iter ) : it( iter ) { }
        reference operator*() const { return **it; }
        pointer operator->() const { return it->get(); }
        iterator& operator++() { ++it; return *this; }
        iterator operator++( int ) { iterator tmp = *this; ++it; return tmp; }
        bool operator==( const iterator& other ) const { return it == other.it; }
        bool operator!=( const iterator& other ) const { return it != other.it; }

    private:
        std::vector< std::unique_ptr< Node > >::iterator it;
    };

    // Const iterator
    struct const_iterator
    {
        using iterator_category = std::forward_iterator_tag;
        using value_type = Node;
        using difference_type = std::ptrdiff_t;
        using pointer = const Node*;
        using reference = const Node&;

        explicit const_iterator( std::vector< std::unique_ptr< Node > >::const_iterator iter ) : it( iter ) {};
        reference operator*() const { return **it; }
        pointer operator->() const { return it->get(); }
        const_iterator& operator++() { ++it; return *this; }
        const_iterator operator++( int ) { const_iterator tmp = *this; ++it; return tmp; }
        bool operator==( const const_iterator& other ) const { return it == other.it; }
        bool operator!=( const const_iterator& other ) const { return it != other.it; }

    private:
        std::vector< std::unique_ptr< Node > >::const_iterator it;
    };

    // Iterator utilities
    iterator begin() { return iterator( m_children.begin() ); }
    iterator end() { return iterator( m_children.end() ); }
    const_iterator begin() const { return const_iterator( m_children.begin() ); }
    const_iterator end() const { return const_iterator( m_children.end() ); }

    Node& operator[]( size_t index );                       // Indexof with a number
    Node& operator[]( const std::string& key );             // Indexof with a string key
    const Node& operator[]( size_t index ) const;           // Indexof with a number (const version)
    const Node& operator[]( const std::string& key ) const; // Indexof with a string key (const version)

    bool IsInvalid() const { return this == &INVALID; }
    NodeType GetNodeType() const { return m_type; }

    bool HasKey() const;
    bool HasValue() const;

    const std::string& GetKey() const;
    const std::string& GetValue() const;
    
    void UpdateKey( const std::string& key );
    void UpdateValue( const std::string& value );
    
    Node& AddNode( const Node& node );
    void RemoveNodeAtIndex( size_t idx );

    Node& AddKey( const std::string& key ) { return AddNode( MakeKey( key ) ); }
    Node& AddValue( const std::string& value ) { return AddNode( MakeValue( value ) ); }
    Node& AddGroup() { return AddNode( MakeGroup() ); }
    
    void Simplify();
    Node SimplifyCopy() const;

    const std::string& GetTextInternal() const { return m_text; }

private:
    NodeType m_type = NodeType::None;
    std::string m_text = "";
    std::vector< std::unique_ptr< Node > > m_children;
};

//-----------------------------------------------------------------------------
// Papr Internal TokenType Enum
// Summary: When a .papr string is broken into tokens, not all tokens are of 
//          the same type. The various types of tokens are represented with 
//          this enum
//-----------------------------------------------------------------------------
enum Papr::Internal::TokenType : uint8_t
{
    None  = 1 << 0,
    Text  = 1 << 1,
    Colon = 1 << 2,
};

//-----------------------------------------------------------------------------
// Papr Internal Token Struct
// Summary: Stores value and other internal metadata for each token that can be 
//          used to parse a string represented in .papr format
//-----------------------------------------------------------------------------
struct Papr::Internal::Token
{
    TokenType type = TokenType::None;
    std::string text = "";
    uint32_t column = 0;
    uint32_t line = 0;
};

//-----------------------------------------------------------------------------
// Papr Parse Function
// Summary: Parse the given string as the data in a .papr file into an 
//          accessible papr object
// data:    A string representing the contents of papr file
// returns: An object representing the parsed papr data
//-----------------------------------------------------------------------------
inline Papr::Node Papr::Parse( const std::string& data ) 
{
    struct InternalStackData 
    {
        Internal::Token token;
        Node* node;
    };

    Node root;

    // The stack keeps track of the current heirarchy of nodes being built
    std::vector< InternalStackData > stack;
    stack.push_back( InternalStackData { .token = Internal::Token { }, .node = &root } );

    auto seekFn = [ &stack ]( uint32_t col, uint32_t token_type_flag ) -> Node*
    {
        // As you look for nodes, pop the elements if it's not something you're 
        // looking for + ignore any with column greater OR EQUAL to token's 
        // column count at any point when looking for a node to attach to, if 
        // the stack becomes empty, log an error and return an empty 
        // intermediate node
        while( stack.size() > 0 )
        {
            InternalStackData& back = stack.back();
            if( back.token.column < col && back.token.type & token_type_flag )
            {
                return back.node;
            }
            stack.pop_back();
        }
        return nullptr;
    };

    for( Internal::Token token : Internal::Tokenize( data ) )
    {
        assert( token.type != Internal::TokenType::None );

        // If token type is text, look for an element in the stack that's of 
        // type "establishing colon" or "leading colon" or "none"
        if( token.type == Internal::TokenType::Text )
        {
            if( Node* node_to_attach_to = seekFn( token.column, Internal::TokenType::Colon | Internal::TokenType::None ) )
            {
                // Don't worry about leaf nodes being connected to other leaf 
                // nodes. We can do that later once all tokens have been parsed
                // insert into the group/none node with token's text as a key 
                // node and add self to the stack
                Node& node = node_to_attach_to->AddNode( Node::MakeKey( token.text ) );
                stack.push_back( InternalStackData { .token = token, .node = &node } );
            }
            else
            {
                std::cerr << "ERROR: Failed to parse given papr data";
                return Node::INVALID;
            }
        }

        // If token type is establishing colon or leading, look for an element 
        // in the stack of type "text"
        else if( token.type == Internal::TokenType::Colon )
        {
            if( Node* node_to_attach_to = seekFn( token.column, Internal::TokenType::Text ) )
            {
                // Insert an intermediate node and add self to the stack
                Node& node = node_to_attach_to->AddNode( Node::MakeGroup() );
                stack.push_back( InternalStackData { .token = token, .node = &node } );
            }
            else
            {
                std::cerr << "ERROR: Failed to parse given papr data";
                return Node::INVALID;
            }
        }
    }

    // Simplify will reorganize the existing parsed structure to be in a much 
    // more simpler and user friendly form. This includes turning childless 
    // key's into values, rerouting groups with a single node and collapsing 
    // multiple value nodes into a single value node separated by an 
    // additional space.
    root.Simplify();
    return root;
}

//-----------------------------------------------------------------------------
// Papr Serialize Function
// Summary: Serialize the given object into a string that can be stored in a 
//          .papr file
// node:    The papr object to serialize
// returns: A string representing the serialized papr object
//-----------------------------------------------------------------------------
inline std::string Papr::Serialize( const Papr::Node& node )
{
    std::string out = "";
    Papr::Node simplified = node.SimplifyCopy();
    Internal::SerializeRecursive( 0, simplified, out );
    return out;
}

//-----------------------------------------------------------------------------
// Papr Internal Trim Function
// Summary: Trims a given parsed token based on the papr standards
// token:   The token to trim
// tokenStartCol: The col in the papr string where the token starts
// returns: A string trimmed based on the papr standards
//-----------------------------------------------------------------------------
inline std::string Papr::Internal::Trim( const std::string& token, size_t tokenStartCol )
{
    size_t start = 0;                   // inclusive
    size_t end  = token.length() - 1;   // excluded

    bool hasFoundFirstChar = false;

    for( size_t i = 0; i < token.length(); i++ )
    {
        const char c = token[ i ];
        if( c != ' ' && !hasFoundFirstChar )
        {
            start = i;
            hasFoundFirstChar = true;
        }

        if( c != ' ' )
        {
            end = i;
        }
    }

    // After trimming the leading and trailing spaces, if the character starts 
    // with a double quote, remove it and also the last double quote
    std::string result = token.substr( start, end - start + 1 );
    if( result.starts_with( '"' ) )
    {
        result = result.substr( 1, result.length() - ( result.ends_with( '"' ) ? 2 : 1 ) );
       
        // The result may contain new lines and in that case according to the papr spec,
        // new lines must be padded with the spaces till it reaches the double quote that 
        // started the token
        size_t newLinePos = result.find( '\n' );
        while ( newLinePos != std::string::npos ) 
        {
            const size_t start = newLinePos + 1;
            if( start >= result.size() )
            {
                break;
            }
            const size_t next = result.find( '\n', start );
            const size_t length = next == std::string::npos ? result.size() - start : next - start;
            result.erase( start, std::min( tokenStartCol, length ) );
            newLinePos = result.find( '\n', start );
        }
    }
    return result;
}

//-----------------------------------------------------------------------------
// Papr Internal Tokenize Function
// Summary: Breaks a given string into tokens that can be used by the parser to 
//          build network of Papr Nodes and Intermediate Nodes
// data:    The papr data in string format
// returns: A list of token with additional metadata
//-----------------------------------------------------------------------------
inline std::vector< Papr::Internal::Token > Papr::Internal::Tokenize( const std::string& data )
{
    // Stores all tokens
    std::vector< Token > tokens;

    // The partial string that gets accumulated as each new character is processed
    std::string partialToken = "";

    // Tracks the column and line count of the current character being 
    // processed. The first character in a document is at column 1, line 1
    uint32_t charCol = 0;
    uint32_t charLine = 1;

    // Variables used to track the column and line count of the first character 
    // in a token being built
    uint32_t tokenStartCol = 0;
    uint32_t tokenStartLine = 0;

    bool inQuotes = false;          // Is the token being parsed inside double quotes?
    bool tokenHasContent = false;   // Does the token have content?
    bool inComment = false;         // Is the content being parsed in a comment?

    // A colon, new line, EOF, or the hashtag symbols (used to represent a comment)
    // are all considered as delimiter
    const auto isDelimiter = []( char c ) -> bool
    {
        return c == ':' || c == '\n' || c == '#' || c == '\0';
    };
    
    for( size_t i = 0; i < data.length(); i++ )
    {
        const char c = data[ i ];                                           // Current character
        const char nc = ( i + 1 ) < data.length() ? data[ i + 1 ] : '\0';   // Next character
        const char pc = ( int )( i - 1 ) >= 0 ? data[ i - 1 ] : '\0';       // Previous character

        // Track column and line number for the new character parsed
        charCol++;
        if( c == '\n' )
        {
            charLine++;
            charCol = 0;
        }

        // Is c the first non-space character in this token?
        const bool isFirstCharInToken = !tokenHasContent && c != ' ' && c != '\n';
        if( isFirstCharInToken )
        {
            tokenHasContent = true;
            tokenStartCol = charCol;
            tokenStartLine = charLine;
        }

        // Only tokens starting with a double quote is considered as being in 
        // quotes and can ignore delimiter inside it
        if( isFirstCharInToken && c == '"' )
        {
            inQuotes = true;
        }
        if( inQuotes && c == '"' && !isFirstCharInToken && pc != '\\' )
        {
            inQuotes = false;
        }

        // Handle single line comments
        if( !inQuotes && c == '#' )
        {
            inComment = true;
        }
        if( inComment && c == '\n' )
        {
            inComment = false;
            partialToken = "";
            tokenHasContent = false;
        }

        if( !inComment && ( inQuotes || !isDelimiter( c ) ) )
        {
            partialToken += c;

            // Is the next character a delimiter ending this token?
            if( !inQuotes && isDelimiter( nc ) && tokenHasContent ) 
            {
                Token t;
                t.type = TokenType::Text;
                t.text = Trim( partialToken, tokenStartCol );
                t.line = tokenStartLine;
                t.column = tokenStartCol;
                tokens.push_back( t );

                partialToken = "";
                tokenHasContent = false;
            }
        }
        else if( !inComment && c == ':' )
        {
            Token t;
            t.type = TokenType::Colon;
            t.line = charLine;
            t.column = charCol;
            tokens.push_back( t );

            partialToken = "";
            tokenHasContent = false;
        }
    }

    return tokens;
}

//-----------------------------------------------------------------------------
// Papr Internal Serialize Recursive Function
// Summary: Serialize the given papr node recursively and stores the result in 
//          the out string
// depth:   The number of spaces from the start of the line the node is
// node:    The papr node to read from and serialize
// out:     The string that the serialized data will be written into
//-----------------------------------------------------------------------------
inline void Papr::Internal::SerializeRecursive( uint32_t depth, const Papr::Node& node, std::string& out )
{
    static auto add_spaces_fn = []( std::string& text, uint32_t count )
    {
        while( count > 0 ) 
        { 
            text += " ";
            count--;
        }
    };

    static auto sanitize_string = []( const std::string& text, uint32_t col ) -> std::string
    {
        if(    text.find( ':' )  != std::string::npos // Colon is reserved for creating parent child relationship between tokens
            || text.find( '#' )  != std::string::npos // Hashtag is reserved for comments
            || text.find( '\n' ) != std::string::npos // Newline is a delimiter
            || text.starts_with( '"' )                // Starting a line with double quotes might get mistaken for a wrapped text (containing double quotes in the middle is okay)
            || text.starts_with( ' ' )                // Leading space is trimmed, the text containing leading spaces must be wrapped (containing spaced in the middle is okay)
            || text.ends_with( ' ' )                  // Trailing space is trimmed, text containing trailing spaces must be wrapped
        )
        {
            std::string out = "";
            for( char c : text )
            {
                if( c == '"' ) 
                { 
                    out += "\\\""; 
                }
                else if( c == '\n' ) 
                { 
                    out += "\n";
                    add_spaces_fn( out, col ); 
                }
                else 
                { 
                    out += c; 
                }
            }
            return '"' + out + '"';
        }
        return text;
    };

    uint32_t child_count = 0;
    for( const Node& child : node )
    {
        NodeType child_type = child.GetNodeType();
        if( child_type == NodeType::Key )
        {
            const std::string& key = child.GetTextInternal();
            const std::string sanitized_key = sanitize_string( key, depth + 1 );
            if( child_count != 0 )
            {
                add_spaces_fn( out, depth );
            }
            out += sanitized_key + ": ";
            SerializeRecursive( depth + sanitized_key.length() + 2, child, out ); // Existing depth + length of key + 2 for ": " appended at the end
        }
        else if( child_type == NodeType::Value )
        {
            const std::string& value = child.GetTextInternal();
            const std::string sanitized_value = sanitize_string( value, depth + 1 );
            if( child_count != 0 )
            {
                add_spaces_fn( out, depth );
            }
            out += sanitized_value + "\n";
            // A value node doesn't have any children, so skipping the recursive call 
        }
        else if( child_type == NodeType::Group )
        {
            if( child_count != 0 )
            {
                add_spaces_fn( out, depth - 2 );
                out += ": ";
            }
            SerializeRecursive( depth, child, out );
        }

        child_count++;
    }
};

//-----------------------------------------------------------------------------
// Papr Node Implementation
//-----------------------------------------------------------------------------
inline Papr::Node Papr::Node::INVALID = Papr::Node();

inline Papr::Node Papr::Node::MakeGroup() 
{
    Node node;
    node.m_type = NodeType::Group;
    return node;
}

inline Papr::Node Papr::Node::MakeKey( const std::string& key )
{
    Node node;
    node.m_type = NodeType::Key;
    node.m_text = key;
    return node;
}

inline Papr::Node Papr::Node::MakeValue( const std::string& value )
{
    Node node;
    node.m_type = NodeType::Value;
    node.m_text = value;
    return node;
}

inline Papr::Node::Node( const Papr::Node& other ) : m_type( other.m_type ), m_text( other.m_text )
{
    for( const auto& child : other.m_children )
    {
        m_children.push_back( std::make_unique< Node >( *child ) );
    }
}

inline Papr::Node::Node( Papr::Node&& other ) : m_type( other.m_type), m_text( std::move( other.m_text ) ), m_children( std::move( other.m_children ) ) { }

inline Papr::Node& Papr::Node::operator=( const Papr::Node& other )
{
    if( this != &other )
    {
        Node copy( other );
        std::swap( *this, copy );
    }
    return *this;
}

inline Papr::Node& Papr::Node::operator=( Papr::Node&& other )
{
    if( this != &other )
    {
        m_type = other.m_type;
        m_text = std::move( other.m_text );
        m_children = std::move( other.m_children );
    }
    return *this;
}

inline Papr::Node& Papr::Node::operator[]( size_t index )
{
    if( index < m_children.size() )
    {
        Node* node_at_index = m_children[ index ].get();
        if( node_at_index )
        {
            return *node_at_index;
        }
    }
    return INVALID;
}

inline const Papr::Node& Papr::Node::operator[]( size_t index ) const
{
    return const_cast< Node& >( *this )[ index ];
}

inline Papr::Node& Papr::Node::operator[]( const std::string& key )
{
    for( std::unique_ptr< Node >& child_unique_ptr : m_children )
    {
        if( Node* child = child_unique_ptr.get() )
        {
            if( child->m_type == NodeType::Key )
            {
                if( child->m_text == key )
                {
                    return *child;
                }
            }
        }
    }
    return INVALID;
}

inline const Papr::Node& Papr::Node::operator[]( const std::string& key ) const
{
    return const_cast< Node& >( *this )[ key ];
}

inline bool Papr::Node::HasKey() const 
{ 
    return m_type == NodeType::Key;
}

inline const std::string& Papr::Node::GetKey() const 
{ 
    if( m_type == NodeType::Key )
    {
        return m_text;
    }
    return INVALID.m_text;
}

inline void Papr::Node::UpdateKey( const std::string& key )
{
    if( m_type == NodeType::Key )
    {
        m_text = key;
    }
}

inline bool Papr::Node::HasValue() const
{
    if( m_type == NodeType::Key || m_type == NodeType::Group )
    {
        if( m_children.size() == 1 ) // must have exactly 1 child
        {
            if( Node* child_value_node = m_children[ 0 ].get() )
            {
                if( child_value_node->m_type == NodeType::Value )
                {
                    return true;
                }
            }
        }
    }
    return false;
}

inline const std::string& Papr::Node::GetValue() const
{
    if( HasValue() )
    {
        return m_children[ 0 ]->m_text;
    }
    return INVALID.m_text;
}

inline void Papr::Node::UpdateValue( const std::string& value )
{
    if( HasValue() )
    {
        m_children[ 0 ]->m_text = value;
    }
}

inline Papr::Node& Papr::Node::AddNode( const Papr::Node& node )
{
    m_children.push_back( std::make_unique< Node >( node ) );
    return *m_children.back().get();
}

inline void Papr::Node::RemoveNodeAtIndex( size_t idx )
{
    m_children.erase( m_children.begin() + idx );
}

inline void Papr::Node::Simplify()
{
    for( Node& child : *this )
    {
        child.Simplify();
    }

    // A key node with only one child and that child is a 'group' node, then 
    // the children of the group node can be directly shortcut to be children of 
    // this key node
    if( m_type == NodeType::Key && m_children.size() == 1 && m_children[ 0 ]->m_type == NodeType::Group )
    {
        auto group_node = std::move( m_children[ 0 ] );
        auto new_children = std::move( group_node->m_children );
        m_children = std::move( new_children );
    }

    // A key or a group node with only a list of value nodes can be simplified 
    // into a combined string separated by a space
    if( ( m_type == NodeType::Key || m_type == NodeType::Group ) && m_children.size() > 1 )
    {
        bool are_all_value_nodes = true;
        for( auto& child : m_children )
        {
            if( child->m_type != NodeType::Value )
            {
                are_all_value_nodes = false;
                break;
            }
        }

        if( are_all_value_nodes )
        {
            size_t count = 0;
            std::string combined_text = "";
            for( auto& child : m_children )
            {
                combined_text += child->m_text + ( count < m_children.size() - 1 ? " " : "" );
                count++;
            }
            m_children.clear();
            AddNode( MakeValue( combined_text ) );
        }
    } 

    // A key node without any children get's transformed into a value node
    if( m_type == NodeType::Key && m_children.size() == 0 )
    {
        m_type = NodeType::Value;
    }
}

inline Papr::Node Papr::Node::SimplifyCopy() const
{
    Node copy = *this;
    copy.Simplify();
    return copy;
}

#endif // PAPR_H
