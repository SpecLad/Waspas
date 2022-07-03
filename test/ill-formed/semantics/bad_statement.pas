program badstmt;
label 1, 3;
        {^ error:unused-label }
var
    j: real;
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

    goto 2;
        {^ error:undefined-label }

    case 0 of
        1.1: ;
       {^ error:non-ordinal-type }

        0,       0: ;
       {^ note }{^ error:duplicate-case }
    end;
end.
