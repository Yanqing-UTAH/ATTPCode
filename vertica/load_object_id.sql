\timing on

drop table if exists wc_object_id;

create table wc_object_id (
    ts int,
    cid int);

copy wc_object_id from local '/uusoc/scratch/raven2/zyzhao/ATTPCode/data/wc-object-id-all.txt' delimiter ' ';

select count(*) from wc_object_id;
