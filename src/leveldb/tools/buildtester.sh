#! /bin/bash
## ./buildtester.sh builder.list leveldb_tag
##    or
## ./buildertest.sh builder.list      ""       tarfile.tgz
##       $0         $1                $2           $3
##
## NOTE: you must manually ssh to each buildbot to get RSA fingerprint
##       into your local known_hosts file before script works

# eleveldb requires knowing which erlang is installed where 
REPO=leveldb

##
##  Subroutines must appear before code using them
##

ssh_command()
{
#   ssh_command "ip address" "command to execute"
    if ssh -q -o 'BatchMode yes' buildbot@$1 "$2"
    then
        echo "success: $2"
    else
        echo "Error on $1 executing $2"
        exit 1
    fi
}

ssh_command_test()
{
#   ssh_command "ip address" "command to execute"
    if ssh -q -o 'BatchMode yes' buildbot@$1 "$2"
    then
        return 0
    else
        return 1
    fi
}

#
# main
#

if [ $# == 2 ]; then
    echo "2 parameters"
    builder_list=$(cut -d ' ' -f 1 $1)
    for builder in $builder_list
    do
        echo "builder: " $builder

        ## remove previous eleveldb instance
        #ssh_command $builder "cd ~/$USER && if [ -d eleveldb ] rm -rf eleveldb"
        echo -n "Start $builder: " >>./builder.log
        date >>./builder.log

        ssh_command $builder "rm -rf ~/$USER/$REPO"
        ssh_command $builder "mkdir -p ~/$USER"
        ssh_command $builder "cd ~/$USER && git clone git@github.com:basho/$REPO"
        ssh_command $builder "cd ~/$USER/$REPO && git checkout $2"

        # freeBSD needs gmake explicitly, otherwise "Missing dependency operator" errors
        #  but other platforms assume "make" is gnumake
        if ssh_command_test $builder "which gmake"
        then 
            ssh_command $builder "cd ~/$USER/$REPO && gmake -j 4"
            echo -n "Test $builder: " >>./builder.log
            date >>./builder.log
            ssh_command $builder "cd ~/$USER/$REPO && export LD_LIBRARY_PATH=. && gmake -j 4 check"
            # freebsd error: util/cache2_test.cc:170: failed: -1 == 201 ... fixed
        else
            ssh_command $builder "cd ~/$USER/$REPO && make -j 4"
            echo -n "Test $builder: " >>./builder.log
            date >>./builder.log
            ssh_command $builder "cd ~/$USER/$REPO && export LD_LIBRARY_PATH=. && make -j 4 check"
        fi

        echo -n "End $builder: " >>./builder.log
        date >>./builder.log
        echo "" >>./builder.log
    done
elif [ $# == 3 ]; then
    echo "3 parameters"
else
    echo " ./buildtester.sh builder.list leveldb_tag"
    echo "    or"
    echo " ./buildertest.sh builder.list \"\" tarfile.tgz"
fi

exit 0
