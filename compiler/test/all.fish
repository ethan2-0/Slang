for dir in (ls -d */)
    echo TEST $dir
    pushd $dir
    fish test.fish
    popd
end
