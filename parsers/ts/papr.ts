// ----------------------------------------------------------------------------
// Author:      Sliptrixx (Hibnu Hishath)
// Date:        2026-05-13
//
// Description: This file contains parser and serializer functions for the 
//              .papr file format
// ----------------------------------------------------------------------------

/** There are a few different types of nodes created when a .papr file is 
 *  parsed. Those types are represent with this enum. */
export type NodeType = 'None' | 'Group' | 'Key' | 'Value';

/** Stores data representing key-value pairs in a .papr file */
export class Node
{
    /** Stores the type of node this represents */
    type : NodeType = 'None';

    /** Stores string value in value nodes or the key value in the key node */
    text : string = "";

    /** A list of nodes that behave as the children of this node */
    children : Node[] = [];

    /** Build a group node */
    static MakeGroup() : Node
    {
        const node : Node = new Node();
        node.type = 'Group';
        return node;
    }

    /** Build a key node with the given key */
    static MakeKey( key : string ) : Node
    {
        const node : Node = new Node();
        node.type = 'Key';
        node.text = key;
        return node;
    }

    /** Build a value node with the given value */
    static MakeValue( value : string ) : Node
    {
        const node : Node = new Node();
        node.type = 'Value';
        node.text = value;
        return node;
    }

    /** Copy the contents of this node into a new node */
    Clone() : Node 
    {
        const node = new Node();
        node.type = this.type;
        node.text = this.text;
        node.children = this.children.map( child => child.Clone() );
        return node;
    }

    /** Get the child node at the given index */
    Get( index : number ) : Node;

    /** Get the child node with the matching key */
    Get( key : string ) : Node;

    /** Get the child node at the given index or has the matching key */
    Get( indexOrKey : number | string ) : Node | null
    {
        if( typeof indexOrKey === 'number' )
        {
            // Get the child node by index
            const index : number = indexOrKey;
            if( index < this.children.length )
            {
                return this.children[ index ]!;
            }
        }
        else
        {
            // Get the child node with the matching key
            const key : string = indexOrKey;
            for( const child of this.children )
            {
                if( child.type === 'Key' && child.text === key )
                {
                    return child;
                }
            }
        }

        return null;
    }

    /** Check if the node has a key */
    HasKey() : boolean
    {
        return this.type === 'Key';
    }

    /** Check if the node has a value */
    HasValue() : boolean
    {
        // A key or a group node is considered to have a value if it has 
        // exactly one value node as a child
        if( ( this.type === 'Key' || this.type === 'Group' ) && this.children.length === 1 )
        {
            return this.children[ 0 ]!.type === 'Value';
        }
        return false;
    }

    /** Get the the key stored in the node */
    GetKey() : string
    {
        if( this.type === 'Key' )
        {
            return this.text;
        }
        return "";
    }

    /** Get the value stored in the node */
    GetValue() : string
    {
        if( this.HasValue() )
        {
            return this.children[ 0 ]!.text;
        }
        if( this.type === 'Value' )
        {
            return this.text;
        }
        return "";
    }

    /** Update the key in the node */
    UpdateKey( key : string ) : boolean
    {
        if( this.type === 'Key' )
        {
            this.text = key;
            return true;
        }
        return false;
    }

    /** Update the value in the node */
    UpdateValue( value : string ) : boolean
    {
        if( this.HasValue() && this.children[ 0 ] )
        {
            this.children[ 0 ].text = value;
            return true;
        }
        return false;
    }

    /** Add a copy of the given node as a child */
    AddNode( node : Node ) : Node
    {
        const copy = node.Clone();
        this.children.push( copy );
        return copy;
    }

    /** Remove the child node at the given index */
    RemoveNodeAtIndex( index : number ) : void
    {
        if( index >= 0 && index < this.children.length )
        {
            this.children.splice( index, 1 );
        }
    }

    /** Add a key node as a child with the given string key */
    AddKey( key : string ) : Node 
    {
        return this.AddNode( Node.MakeKey( key ) );
    }

    /** Add a value node as a child with the given string */
    AddValue( value : string ) : Node
    {
        return this.AddNode( Node.MakeValue( value ) );
    }

    /** Add a group node as a child and return a reference to the added node */
    AddGroup() : Node
    {
        return this.AddNode( Node.MakeGroup() );
    }

    /** Simplify the current papr node data structure */
    Simplify() : Node
    {
        // Simplify all the children first
        for( const child of this.children )
        {
            child.Simplify();
        }

        // A key node with only one child and that child is a 'group' node, 
        // then the children of the group node can be directly shortcut to be 
        // children of this 'key' node
        if( this.type === 'Key' && this.children.length === 1 && this.children[ 0 ]!.type === 'Group' )
        {
            this.children = this.children[ 0 ]!.children;
        }

        // A key or a group node with only a list of value nodes can be 
        // simplified into a combined string separated by a space
        if( ( this.type === 'Key' || this.type === 'Group' ) && this.children.length > 1 )
        {
            const is_all_values : boolean = this.children.every( child => child.type === 'Value' );
            if( is_all_values )
            {
                const combined : string = this.children.map( child => child.text ).join( ' ' );
                this.children = [ Node.MakeValue( combined ) ];
            }
        }

        // A key node without any children gets transformed into a value node
        if( this.type === 'Key' && this.children.length === 0 )
        {
            this.type = 'Value';
        }

        return this;
    }

    /** Custom iterator */
    *[Symbol.iterator](): Iterator< Node > 
    {
        for( const child of this.children )
        {
            yield child;
        }
    }
}

/** When a .papr string is broken into tokens, not all tokens are of the same 
 *  type. The various type of tokens are represented with this enum. */
type TokenType = 'None' | 'Text' | 'Colon';

/** Stores values and other internal metadata for each token that can be used 
 * to parse a string represented in .papr format */
interface Token
{
    type : TokenType;
    text : string;
    line : number;
    column : number;
}

/** Internal function used to trim a token getting parsed based on papr specs */
function TokenTrim( token : string, token_start_col : number ) : string
{
    // Start by trimming the leading and trailing spaces
    let result : string = token.trim();

    // After trimming the leading and trailing spaces, if the characters with a 
    // double quote, remove it and also the last double quote
    if( result.startsWith( '"' ) )
    {
        result = result.substring( 1, result.length - ( result.endsWith( '"' ) ? 1 : 0 ) );

        // The result may contain new lines and in that case, according to the 
        // papr specs, new lines must be padded with spaces till it reaches the 
        // double quote that started the token
        let new_line_pos : number = result.indexOf( '\n' );
        while( new_line_pos !== -1 )
        {
            const start : number = new_line_pos + 1;
            if( start >= result.length ) 
            { 
                break; 
            }

            const next : number = result.indexOf( '\n', start );
            const length : number = ( next === -1 ? result.length : next ) - start;
            result = result.substring( 0, start ) + result.substring( start + Math.min( token_start_col, length ) );

            new_line_pos = result.indexOf( '\n', start );
        }
    }

    return result;
}

/** Internal function that breaks a given string into tokens that can be used 
 * by the parser to build a network of papr nodes and intermediate nodes */
function Tokenize( data : string ) : Token[]
{
    // Stores all the tokens
    let tokens : Token[] = [];

    // The partial string that gets accumulated as each new character is 
    // processed
    let partial_token : string = "";

    // Tracks the column and line count of the current character being 
    // processed. The first character in a document is at column 1, line 1
    let char_col : number = 0;
    let char_line : number = 1;

    // Variables used to track the column and line count of the first character 
    // in a token being built
    let token_start_col : number = 0;
    let token_start_line : number = 0;

    // Is the token being parsed inside double quotes?
    let in_quotes : boolean = false;

    // Does the token have any content?
    let token_has_content : boolean = false;

    // Is the content being parsed a comment?
    let in_comment : boolean = false;

    // A colon, new line, EOF, or the hashtag symbols (used to represent a 
    // comment) are all considered as delimiters
    function isDelimiter( c : string ) : boolean 
    {
        return c === ':' || c === '\n' || c === '#' || c === '\0';
    }

    // Loop through each character and begin the tokenization process
    for( let i = 0; i < data.length; i++ )
    {
        const c : string = data[ i ]!;                                       // Current character
        const nc : string = ( i + 1 ) < data.length ? data[ i + 1 ]! : '\0'; // Next character
        const pc : string = ( i - 1 ) >= 0 ? data[ i - 1 ]! : '\0';          // Previous character
    
        // Track column and line number for the new character parsed
        char_col++;
        if( c === '\n' )
        {
            char_line++;
            char_col = 0;
        }

        // Is c the first non-space character in this token?
        const is_first_char_in_token : boolean = !token_has_content && c !== ' ' && c !== '\n';
        if( is_first_char_in_token )
        {
            token_has_content = true;
            token_start_col = char_col;
            token_start_line = char_line;
        }

        // Only tokens starting with a double quote is considered as being in 
        // quotes and can ignore delimiters inside it.
        if( is_first_char_in_token && c === '"' )
        {
            in_quotes = true;
        }

        // The token is considered to be no longer in quotes when it encounters 
        // another double quote character (as long it's not prepended with an 
        // escape sequence character)
        if( in_quotes && c === '"' && !is_first_char_in_token && pc !== '\\' )
        {
            in_quotes = false;
        }

        // Handle single line comments
        if( !in_quotes && c === '#' )
        {
            in_comment = true;
        }
        if( in_comment && c === '\n' )
        {
            in_comment = false;
            partial_token = "";
            token_has_content = false;
        }

        // Start appending to the partial token
        if( !in_comment && ( in_quotes || !isDelimiter( c ) ) )
        {
            partial_token += c;

            // Is the next character a delimiter ending this token?
            if( !in_quotes && isDelimiter( nc ) && token_has_content )
            {
                tokens.push( { 
                    type: 'Text',
                    text: TokenTrim( partial_token, token_start_col ),
                    line: token_start_line,
                    column: token_start_col
                } );

                partial_token = "";
                token_has_content = false;
            }
        }
        else if( !in_comment && c === ':' )
        {
            tokens.push( { 
                type: 'Colon',
                text: "",
                line: char_line,
                column: char_col
            } );

            partial_token = "";
            token_has_content = false;
        }
    }

    return tokens;
}

/** Internal function that serializes a papr node recursively and returns the 
 *  out value. With the c++ implementation, I was able to pass the out string 
 *  as a reference but since that's not possible in typescript/javascript, I'll 
 *  be returning that value instead */
function SerializeRecursive( depth : number, node : Node, out : string ) : string
{
    function sanitizeString( text : string, col : number ) : string
    {
        if(    text.indexOf( ':' )  !== -1 // Colon is reserved for creating parent child relationship between tokens
            || text.indexOf( '#' )  !== -1 // Hashtag is reserved for comments
            || text.indexOf( '\n' ) !== -1 // Newline is a delimiter
            || text.startsWith( '"' )      // Starting a line with double quotes might get mistaken for a wrapped text (containing double quotes in the middle is okay) 
            || text.startsWith( ' ' )      // Leading spaces are trimmed, text containing leading spaces must be wrapped (containing spaces in the middle is okay)
            || text.endsWith( ' ' ) )      // Trailing spaces are trimmed, text containing trailing spaces must be wrapped
        {
            let out = "";
            for( const c of text )
            {
                if( c === '"' )
                {
                    out += "\\\"";
                }
                else if( c === '\n' )
                {
                    out += `\n${ " ".repeat( col ) }`;
                }
                else
                {
                    out += c;
                }
            }
            return `"${ out }"`;
        }
        return text;
    }

    let child_count : number = 0;
    for( const child of node )
    {
        if( child.type === 'Key' )
        {
            const sanitized_key : string = sanitizeString( child.text, depth + 1 );
            if( child_count !== 0 )
            {
                out += " ".repeat( depth );
            }
            out += `${ sanitized_key }: `;
            out = SerializeRecursive( depth + sanitized_key.length + 2, child, out ); // Existing depth + length of key + 2 for ": " appended at the end
        }
        else if( child.type === 'Value' )
        {
            const sanitized_value : string = sanitizeString( child.text, depth + 1 );
            if( child_count !== 0 )
            {
                out += " ".repeat( depth );
            }
            out += `${ sanitized_value }\n`;
            // A value node doesn't have any children, so skipping recursive call
        }
        else if( child.type === 'Group' )
        {
            if( child_count !== 0 )
            {
                out += `${ " ".repeat( depth - 2 ) }: `;
            }
            out = SerializeRecursive( depth, child, out );
        }

        child_count++;
    }

    return out;
}

/** Internal function used to convert a json object to a list of papr nodes */
function FromJsonRecursive( obj : any ) : Node[]
{
    // Stores the output list of nodes
    const node_list : Node[] = [];

    // Is it a list?
    if( Array.isArray( obj ) )
    {
        for( const item of obj )
        {
            const group_node = Node.MakeGroup();
            const value_nodes = FromJsonRecursive( item );
            for( const node of value_nodes )
            {
                group_node.AddNode( node );
            }
            node_list.push( group_node );
        }
    }

    // Is it a JSON object?
    else if( obj !== null && typeof obj === 'object' )
    {
        for( const [ key, value ] of Object.entries( obj ) )
        {
            const key_node = Node.MakeKey( key );
            const value_nodes = FromJsonRecursive( value );
            for( const node of value_nodes )
            {
                key_node.AddNode( node );
            }
            node_list.push( key_node );
        }
    }

    // Neither a list or an object, must be a primitive. Making it a value node 
    // and returning back to caller
    else
    {
        const value_node = Node.MakeValue( String( obj ) );
        node_list.push( value_node );
    }

    return node_list;
}

/** An internal namespace, giving optional public access to internal function. 
 *  Not recommended using it, but if you for some reason want to, I'm not going 
 *  to block it. */
export const Internal = { TokenTrim, Tokenize, SerializeRecursive, FromJsonRecursive } as const;

/** Parse the given string in .papr file format into an accessible papr object */
export function Parse( data : string ) : Node | null
{
    // Used to represent the objects of the internal parser stack structure
    interface InternalStackData
    {
        node : Node;
        token : Token;
    }

    // The root of the papr tree structure. This is the only valid node with 
    // the 'None' node type.
    const root : Node = new Node();

    // The stack keeps track of the current hierarchy of nodes being built
    const stack : InternalStackData[] = []
    stack.push( { node: root, token: { type: 'None', text: "", line: 0, column: 0 } })

    function seekFn( col : number, token_types : TokenType[] ) : Node | null
    {
        // As you look for nodes, pop the elements from the stack if it's not 
        // something you're look for + ignore any with column greater OR EQUAL 
        // to token's column count at any when looking for a node to attach to. 
        // If the stack become empty, return an empty intermediate node.
        while( stack.length > 0 )
        {
            const back : InternalStackData = stack[ stack.length - 1 ]!;
            if( back.token.column < col && token_types.includes( back.token.type ) )
            {
                return back.node;
            }
            stack.pop();
        }
        return null;
    } 

    for( const token of Tokenize( data ) )
    {
        // If the token type is a text, look for an element in the stack that's 
        // of type 'Colon' or 'None'
        if( token.type === 'Text' )
        {
            const node_to_attach_to = seekFn( token.column, [ 'Colon', 'None' ] );
            if( node_to_attach_to === null )
            {
                console.error( "Failed to parse given papr data" );
                return null;
            }

            // Don't worry about leaf nodes being connected to other leaf nodes. 
            // We can deal with that later once all the tokens have been parsed 
            // and inserted into the group/none nodes with token's text as a key 
            // node
            const node = node_to_attach_to.AddNode( Node.MakeKey( token.text ) );
            stack.push( { node: node, token: token } );
        }

        // If the token is a colon, look for an element in the stack that's of 
        // type 'Text'
        else if( token.type === 'Colon' )
        {
            const node_to_attach_to = seekFn( token.column, [ 'Text' ] );
            if( node_to_attach_to === null )
            {
                console.error( "Failed to parse given papr data" );
                return null;
            }

            // Insert an intermediate node and it to the stack
            const node = node_to_attach_to.AddNode( Node.MakeGroup() );
            stack.push( { node: node, token: token } );
        }
    }

    // Simplify will reorganize the existing parsed structure to be in a much 
    // more simpler and user friendly form. This includes turning childless 
    // keys into values, rerouting groups with a single node and collapsing 
    // multiple value nodes into a single value node separated by an 
    // additional space.
    root.Simplify();
    return root;
}

/** Serialize the given object into a string that can stored in a .papr file */
export function Serialize( node : Node ) : string
{
    let simplified : Node = node.Clone().Simplify();
    return SerializeRecursive( 0, simplified, "" );
}

/** Convert a given papr node to json */
export function ToJson( node : Node, value_as_primitive : boolean = true ) : any
{
    // Collapse the given list of nodes to a json adjacent structure if possible
    function m_Flatten( nodes : Node[] ) : any
    {
        // If every node is a group, then we need to wrap it up in a list
        if( nodes.every( ( node ) => node.type === 'Group' ) )
        {
            const list = [];
            for( const node of nodes )
            {
                list.push( ToJson( node, value_as_primitive ) );
            }
            return list.length === 1 ? list[ 0 ] : list;
        }

        // If every node is a key, combine the result into a single object
        if( nodes.every( ( node ) => node.type === 'Key' ) )
        {
            const object = {};
            for( const node of nodes )
            {
                Object.assign( object, ToJson( node, value_as_primitive ) );
            }
            return object;
        }

        // Lastly, if every node is a value combine them into a space separated 
        // strings. The one exception is going to be a singular value, then 
        // return as is
        if( nodes.every( ( node ) => node.type === 'Value' ) )
        {
            if( nodes.length === 1 ) 
            { 
                return ToJson( nodes[ 0 ] );
            }
            return nodes.map( ( node ) => node.text ).join( ' ' );
        }

        // Contains a mixed bag of things, this isn't really something that I 
        // expected. If it happens lets deal with that in the future, but for 
        // now returning null
        return null;
    }

    if( node.type === "Value" )
    {
        // A value node has no children, so we only care about its text value. 
        // We will also attempt to convert the string into a primitive type 
        // values when requested
        if( value_as_primitive && node.text.length > 0 )
        {
            if( node.text === "true" )      { return true;      }
            if( node.text === "false" )     { return false;     }
            if( node.text === "null" )      { return null;      }
            if( node.text === "undefined" ) { return undefined; }

            const number = Number( node.text );
            if( !isNaN( number ) ) 
            { 
                return number; 
            }
        }

        return node.text;
    }
    else if( node.type === "Key" )
    {
        return { [ node.text ]: m_Flatten( node.children ) };
    }
    
    // It's either a 'group' or 'none' node, simply collapse any children and 
    // return back. The 'none' node is currently only reserved for the root node
    return m_Flatten( node.children );;
}

/** Convert the given json into a papr node */
export function FromJson( json : object ) : Node
{
    const root = new Node();
    for( const node of FromJsonRecursive( json ) )
    {
        root.AddNode( node );
    }
    return root.Simplify();
}
