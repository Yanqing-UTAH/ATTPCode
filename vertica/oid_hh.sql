
with t as (
    select * from wc_object_id where ts <= 901489153
)
select count(*) from (
    select cid, count(*) as cnt
    from t
    group by cid
    having count(*) >= 0.01 * (
        select count(*) from t
    )
) A;
