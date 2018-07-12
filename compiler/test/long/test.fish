echo "Ignoring test `long` for the moment, since it takes a very long time."
exit

function areeq
    [ (cat $argv[1] | md5sum) = (cat $argv[2] | md5sum) ]
end

set testdir (pwd)
pushd ../..
python3 emitter.py --ast --segments $testdir/input.slg > $testdir/output.tmp
popd

if areeq output.tmp output; and areeq input.slb expected.slb
    echo SUCCESS
else
    echo FAILURE
end
