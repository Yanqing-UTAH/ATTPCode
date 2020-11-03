
with t as (
    select * from wc_client_id where ts <= 894041293
)
select count(*) from (
    select cid, count(*) as cnt
    from t
    group by cid
    having count(*) >= 0.0002 * (
        select count(*) from t
    )
) A;
