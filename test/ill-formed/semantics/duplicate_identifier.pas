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

    recordSelector = 1..2;
    recordType = record
        a: integer;
       {^ note }
        a: real;
       {^ error:duplicate-identifier }

        b: integer;
       {^ note }

        c: real;
       {^ note }

        case d: recordSelector of
            {^ note }
            1: (case b: recordSelector of 1: (); 2: ());
                    {^ error:duplicate-identifier }
            2: (
                c: text;
               {^ error:duplicate-identifier }
                d: boolean;
               {^ error:duplicate-identifier }
            );
    end;
begin
end.
