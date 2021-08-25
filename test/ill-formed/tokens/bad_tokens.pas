 " { unknown character }
{^ error:invalid-token}
 23 abc { these are fine }
 23abc { integer directly followed by an identifier }
  {^ error:missing-separator}
 23.45abc { real directly followed by an identifier }
     {^ error:missing-separator}
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
