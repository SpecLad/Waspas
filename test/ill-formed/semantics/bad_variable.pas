program badvar;
const
    notType = 1;
   {^ note }
var
    unknownType: unknown;
                {^ error:undefined-identifier }
    wrongIdKindBuiltin: maxint;
                       {^ error:wrong-identifier-kind }
    wrongIdKind: notType;
                {^ error:wrong-identifier-kind }
    wrongIdKindSelf: wrongIdKindSelf;
   {^ note }        {^ error:wrong-identifier-kind }

    { various variations on use-before-definition }
    ubdVar: laterVar;
           {^ error:use-before-definition }
    ubdEnumVar: laterEnumVar;
               {^ error:use-before-definition }
    ubdProcedure: laterProcedure;
                 {^ error:use-before-definition }
    ubdFunction: laterFunction;
                {^ error:use-before-definition }

    laterVar: (laterEnumVar);
   {^ note }  {^ note }
procedure laterProcedure; begin end;
         {^ note }
function laterFunction: integer; begin laterFunction := 0 end;
        {^ note }
begin
end.
