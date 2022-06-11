program badconst(input);
                {^ note }
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

    wrongIdKindBuiltin = integer;
                        {^ error:wrong-identifier-kind }
    wrongIdKind = input;
                 {^ error:wrong-identifier-kind }

    selfRef = selfRef;
             {^ error:circular-definition }

    { various variations on use-before-definition }
    ubdConst = laterConst;
              {^ error:use-before-definition }
    laterConst = 0;
   {^ note }
    ubdType = laterType;
             {^ error:use-before-definition }
    ubdEnum = laterEnum;
             {^ error:use-before-definition }
    ubdVar = laterVar;
            {^ error:use-before-definition }
    ubdEnumVar = laterEnumVar;
                {^ error:use-before-definition }
    ubdProcedure = laterProcedure;
                  {^ error:use-before-definition }
    ubdFunction = laterFunction;
                 {^ error:use-before-definition }
type
    laterType = (laterEnum);
   {^ note }    {^ note }
var
    laterVar: (laterEnumVar);
   {^ note }  {^ note }
procedure laterProcedure; begin end;
         {^ note }
function laterFunction: integer; begin laterFunction := 0 end;
        {^ note }
begin
end.
