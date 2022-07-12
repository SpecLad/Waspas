program badstmt;
label 1, 3;
        {^ error:unused-label }
type
    rec = record
        f: integer;
    end;
var
    j: real;
    k: integer;
    r: rec;
procedure p;
    begin
        for k := 1 to 10 do;
           {^ error:undefined-identifier }
    end;
begin
    1: ;
   {^ note }
    1: ;
   {^ error:ambiguous-label }
    2: ;
   {^ error:undefined-label }

    for i := 1 to 10 do;
       {^ error:undefined-identifier }

    for j := 1 to 10 do;
       {^ error:non-ordinal-type }

    with r do
        for f := 1 to 10 do;
           {^ error:wrong-identifier-kind }

    goto 2;
        {^ error:undefined-label }

    case 0 of
        1.1: ;
       {^ error:non-ordinal-type }

        0,       0: ;
       {^ note }{^ error:duplicate-case }
    end;

    with k do;
        {^ error:non-record-type }

    if 0 then;
      {^ error:non-boolean-type }

    repeat until 0;
                {^ error:non-boolean-type }

    while 0 do;
         {^ error:non-boolean-type }
end.
