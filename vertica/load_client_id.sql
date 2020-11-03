\timing on

drop table if exists wc_client_id;

create table wc_client_id (
    ts int,
    cid int);

copy wc_client_id from local '/uusoc/scratch/raven2/zyzhao/ATTPCode/data/wc-client-id-all.txt' delimiter ' ';

select count(*) from wc_client_id;
