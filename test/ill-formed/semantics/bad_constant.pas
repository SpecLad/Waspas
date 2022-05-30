program badconst;
const
    a = -unknown;
        {^ error:undefined-identifier }
    b = 123;
    c = '1';
    d = 'many';
    e = +c;
       {^ error:type-mismatch }
    f = -d;
       {^ error:type-mismatch }

    { various variations on use-before-definition }
    ubdSelf = ubdSelf;
   {^ note}  {^ error:use-before-definition }
    ubdConst = id1;
              {^ error:use-before-definition }
    id1 = 0;
   {^ note }
    ubdType = id2;
             {^ error:use-before-definition }
    ubdEnum = id3;
             {^ error:use-before-definition }
    ubdVar = id4;
            {^ error:use-before-definition }
    ubdEnumVar = id5;
                {^ error:use-before-definition }
    ubdProcedure = id6;
                  {^ error:use-before-definition }
    ubdFunction = id7;
                 {^ error:use-before-definition }
type
    id2 = integer;
   {^ note }
    enum = (id3);
           {^ note }
var id4: integer;
   {^ note }
    enumVar: (id5);
             {^ note }
procedure id6; begin end;
         {^ note }
function id7: integer; begin id7 := 0 end;
        {^ note }
begin
end.
