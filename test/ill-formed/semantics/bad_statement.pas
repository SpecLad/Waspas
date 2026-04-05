program badstmt(input);
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

    threatenedInProcedure, threatenedInBody: integer;
procedure p(function fp: integer);
    begin
        for k := 1 to 10 do;
           {^ error:undefined-identifier }

        threatenedInProcedure := 0;
       {^ note }

        goto 4;
            {^ error:disallowed-goto-target }

        fp;
       {^ error:wrong-identifier-kind }
    end;
function f: integer; begin f := 0 end;
function writerfunc(var i: integer): integer;
    begin writerfunc := 0; end;
procedure writerproc(var i: integer);
    begin end;
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

    for threatenedInBody := 1 to 10 do
       {^ note }
        for threatenedInBody := 1 to 10 do;
           {^ error:threatened-control-variable }

    for threatenedInBody := 1 to 10 do
       {^ note }
        writerproc(threatenedInBody);
                  {^ error:threatened-control-variable }

    for threatenedInBody := 1 to 10 do
       {^ note }
        k := writerfunc(threatenedInBody);
                       {^ error:threatened-control-variable }

    for threatenedInBody := 1 to 10 do
       {^ note }
        read(threatenedInBody);
            {^ error:threatened-control-variable }

    for threatenedInBody := 1 to 10 do
       {^ note }
        read(input, threatenedInBody);
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

    unknown;
   {^ error:undefined-identifier }
    j;
   {^ error:wrong-identifier-kind }
    f;
   {^ error:wrong-identifier-kind }
end.
