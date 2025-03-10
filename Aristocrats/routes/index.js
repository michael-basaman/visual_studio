var express = require('express');
var router = express.Router();

const { Pool } = require('pg');
const pool = new Pool({
    user: 'trade',
    host: 'localhost',
    database: 'trade',
    password: 'trade',
    port: 5432,
});

/* GET home page. */
router.get('/', async (req, res) => {
    try {
        const client = await pool.connect();
        const equities = await client.query(`
            select e1.ticker, e1.price_close, a.homepage_url, a.name
            from equities e1
            inner join
                (select e2.ticker, max(e2.price_date) as max_price_date
                from equities e2
                group by e2.ticker) agg
            on e1.ticker = agg.ticker
            and e1.price_date = agg.max_price_date
            inner join aristocrats a
            on agg.ticker = a.ticker
            order by e1.ticker`);

        const dividends = await client.query(`
            select d1.ticker, d1.cash_amount
            from dividends d1
            inner join
	            (select d2.ticker, max(d2.pay_date) as max_pay_date
	             from dividends d2
	             group by d2.ticker) agg
            on d1.ticker = agg.ticker
            and d1.pay_date = agg.max_pay_date
            order by d1.ticker`);

        const dividend_map = new Map();

        for (const dividend of dividends.rows) {
            dividend_map.set(dividend.ticker, dividend.cash_amount);
        }

        const aristocrats = [];

        for (const equity of equities.rows) {
            let cash_amount = 0;
            if (dividend_map.has(equity.ticker)) {
                cash_amount = dividend_map.get(equity.ticker);
            }

            let yield = 0.0 ? Math.abs(equity.price_close) < 0.01 : 400.0 * cash_amount / equity.price_close;

            const aristocrat = {
                ticker: equity.ticker,
                yieldFloat: yield,
                yield: yield.toFixed(2),
                homepage_url: equity.homepage_url,
                name: equity.name
            };

            aristocrats.push(aristocrat);
        }
        
        client.release();

        aristocrats.sort((a, b) => {
            if (a.yieldFloat < b.yieldFloat) {
                return 1;
            }
            if (a.yieldFloat > b.yieldFloat) {
                return -1;
            }
            return 0;
        });

        res.render('index', { aristocrats: aristocrats });
    } catch (err) {
        console.error(err);
        res.status(500).send('Server error');
    }
});

module.exports = router;
