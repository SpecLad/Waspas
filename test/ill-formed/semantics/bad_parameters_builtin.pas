program bpb;
var
    fi: file of integer;
begin
    write;
        {^ error:parameter-count-mismatch }
    write(fi);
           {^ error:parameter-count-mismatch }

    write(fi:3, 1);
           {^ error:disallowed-parameter-form }

    write(fi, 1:3);
              {^ error:disallowed-parameter-form }

    write(fi, 1.1);
             {^ error:type-mismatch }
end.
