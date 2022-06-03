program dup;
const
    a = 1;
   {^ note }
    a = 1;
   {^ error:duplicate-identifier }
type
    b = integer;
   {^ note }
    b = real;
   {^ error:duplicate-identifier }
    enumType = (c,        c);
               {^ note } {^ error:duplicate-identifier }

    recordType = record
        a: integer;
       {^ note }
        a: real;
       {^ error:duplicate-identifier }
    end;
begin
end.
