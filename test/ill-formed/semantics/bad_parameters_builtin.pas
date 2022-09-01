program bpb(output);
var
    fi: file of integer;
    t: text;
begin
    write;
        {^ error:parameter-count-mismatch }
    write(fi);
           {^ error:parameter-count-mismatch }

    write(fi.x, 1);
           {^ error:non-record-type } { testing that the error is only emitted once }

    write(fi:3, 1);
           {^ error:disallowed-parameter-form }

    write(fi, 1:3);
              {^ error:disallowed-parameter-form }

    write(fi, 1.1);
             {^ error:type-mismatch }

    write(t);
          {^ error:parameter-count-mismatch }

    write(t.x, 1);
          {^ error:non-record-type } { testing that the error is only emitted once }

    write(t:3, 1);
          {^ error:disallowed-parameter-form }

    write(t, t);
            {^ error:type-mismatch }

    write(t, 1:1.1);
              {^ error:type-mismatch }

    write(t, 1:1:1.1);
                {^ error:disallowed-parameter-form }

    write(t, 1.1:1:1.1);
                  {^ error:type-mismatch }

    write(nil);
         {^ error:type-mismatch }

    write(1:1.1);
           {^ error:type-mismatch }

    write(1:1:1.1);
             {^ error:disallowed-parameter-form }

    write(1.1:1:1.1);
               {^ error:type-mismatch }
end.
