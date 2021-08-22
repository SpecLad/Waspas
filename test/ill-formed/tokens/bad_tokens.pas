 " { unknown character }
{^ error:invalid-token}
 23 abc { these are fine }
 23abc { integer directly followed by an identifier }
{^ error:invalid-token}
 23.45abc { real directly followed by an identifier }
   {^ error:invalid-token}
    { The error is here instead of the start of the number, because
      when the lexer fails to lex the entire number as a real,
      it lexes the integer part as an integer, and the dot as the operator,
      and only then fails on the fractional part.
      The lexer could be improved to fail earlier.
    }
 'unclosed string
{^ error:invalid-token}
 '
{^ error:invalid-token}
 { "nested" (* comment *) }
                         {^ error:invalid-token}
  { ASCII SOH character }
{^ error:invalid-token}
 '' { empty string }
{^ error:invalid-token}
