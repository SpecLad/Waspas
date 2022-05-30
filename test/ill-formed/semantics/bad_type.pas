program badtype;
type
    a = unknown;
       {^ error:undefined-identifier }

    { various variations on use-before-definition }
    ubdSelf = ubdSelf;
   {^ note}  {^ error:use-before-definition }
    ubdType = laterType;
             {^ error:use-before-definition }
    ubdEnum = laterEnum;
             {^ error:use-before-definition }
    laterType = (laterEnum);
   {^ note }    {^ note }
    ubdVar = laterVar;
            {^ error:use-before-definition }
    ubdEnumVar = laterEnumVar;
                {^ error:use-before-definition }
    ubdProcedure = laterProcedure;
                  {^ error:use-before-definition }
    ubdFunction = laterFunction;
                 {^ error:use-before-definition }
var
    laterVar: (laterEnumVar);
   {^ note }  {^ note }
procedure laterProcedure; begin end;
         {^ note }
function laterFunction: integer; begin laterFunction := 0 end;
        {^ note }
begin
end.
