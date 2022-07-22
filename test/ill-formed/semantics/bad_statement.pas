program badstmt;
label 1, 3, 4;
        {^ error:unused-label }
type
    rec = record
        f: integer;
    end;
var
    j: real;
    k: integer;
    r: rec;

    threatenedInProcedure: integer;
procedure p;
    begin
        for k := 1 to 10 do;
           {^ error:undefined-identifier }

        threatenedInProcedure := 0;
       {^ note }

        goto 4;
            {^ error:disallowed-goto-target }
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

    for k := 1.1 to 10 do;
            {^ error:type-mismatch }

    for k := 1 to 1.1 do;
                 {^ error:type-mismatch }

    for threatenedInProcedure := 1 to 10 do;
       {^ error:threatened-control-variable }

    goto 2;
        {^ error:undefined-label }

    goto 4;
        {^ error:disallowed-goto-target }

    begin 4: end;

    case 1.1 of 1.1: ; end;
        {^ error:non-ordinal-type }

    case 0 of
        1.1: ;
       {^ error:type-mismatch }

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
