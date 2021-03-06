#include <linux/slab.h>
#include <mach/clk.h>

static inline uint32_t pll_clk_lock_check(uint32_t pll_type)
{
	return __clk_readl(PLL_LOCKED_STATUS(pll_type));
}

static inline void pll_gate_ctrl(uint32_t pll_type, uint32_t mode)
{
	uint32_t val;

	val = __clk_readl(PLL_LOAD_AND_GATE(pll_type));
	val &= ~(1 << PLL_OUT_GATE);
	val |= mode << PLL_OUT_GATE;
	__clk_writel(val, PLL_LOAD_AND_GATE(pll_type));
}

static inline void pll_parameter_set(uint32_t pll_type, uint32_t low_byte,
		uint32_t high_byte)
{
	__clk_writel(low_byte, PLL_PARAMETER_LOW(pll_type));
	__clk_writel(high_byte, PLL_PARAMETER_HIGH(pll_type));
}

static inline void pll_load_para_control(uint32_t pll_type,
		uint32_t mode)
{
	uint32_t val;
	val = __clk_readl(PLL_LOAD_AND_GATE(pll_type));
	val &= ~(1 << PLL_LOAD_PA);
	val |= mode << PLL_LOAD_PA;
	__clk_writel(val, PLL_LOAD_AND_GATE(pll_type));
}

static inline void pll_output_ctrl(uint32_t pll_type, uint32_t mode)
{
	__clk_writel(mode, PLL_ENABLE(pll_type));
}

static inline uint32_t pll_para_to_integr(uint32_t bypass, uint32_t od,
		uint32_t r, uint32_t f)
{
	return ((bypass & 0x1) << PLL_BYPASS) | ((od & 0x3) << PLL_OD) |
		 ((r & 0x1f) << PLL_R) | ((f & 0x7f) << PLL_F);
}

static void read_pll_para(struct pll_clk_info *info, enum pll_type type)
{
	uint32_t para;

	para = (__clk_readl(PLL_PARAMETER_HIGH(type)) << 8) |
			(__clk_readl(PLL_PARAMETER_LOW(type)));
	info->bypass = (para >> PLL_BYPASS) & 0x1;
	info->od = (para >> PLL_OD) & 0x3;
	info->r = (para >> PLL_R) & 0x1f;
	info->f = (para >> PLL_F) & 0x7f;
}

/* for apll to 12M when cpufreq */
static int apll_reset(uint32_t value)
{
#if 0
	int i;
	/* out=ref, src=osc */
	__clk_writel(0, APLL_SRC_STATUS);
	/* disable load, but preserve output */
	__clk_writel(0, APLL_LOAD_AND_GATE);
	/* disable pll */
	__clk_writel(0, APLL_ENABLE);
	/* prepare: change parameters */
	__clk_writel(value & 0xff, APLL_PARAMETER_LOW);
	__clk_writel(value >> 8, APLL_PARAMETER_HIGH);
	/* enable load */
	__clk_writel(2, APLL_LOAD_AND_GATE);
	/* enable pll */
	__clk_writel(1, APLL_ENABLE);
	for (i = 0; (i < 6666) && !(__clk_readl(APLL_LOCKED_STATUS) & 1);
		i++)
		;
	__clk_writel(2, APLL_SRC_STATUS);
#else
	/* disable load, but preserve output */
	__clk_writel(0, APLL_LOAD_AND_GATE);
	/* prepare: change parameters */
	__clk_writel(value & 0xff, APLL_PARAMETER_LOW);
	__clk_writel(value >> 8, APLL_PARAMETER_HIGH);
	__clk_writel(1, 0xc18);
	while(__clk_readl(0xc18));
#endif
	return 0;
}

static uint32_t pll_para_to_rate(struct pll_clk_info *info, uint32_t rate)
{
	return (rate * 2 * (info->f + 1)) >> (info->od);
}

static int imapx_pll_is_enabled(struct clk_hw *hw)
{
	struct imapx_pll_clk *pll = to_clk_pll(hw);
	uint32_t val;

	val = __clk_readl(PLL_ENABLE(pll->type));
	val = val & 0x1;

	return val;
}

static int imapx_pll_enable(struct clk_hw *hw)
{
	struct imapx_pll_clk *pll = to_clk_pll(hw);
	unsigned long flags = 0;

	//if (pll->type == APLL)
	if (!strcmp(pll->hw.clk->name,"apll"))
		return 0;

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);

	__clk_writel(ENABLE, PLL_ENABLE(pll->type));
#if defined(CONFIG_APOLLO_FPGA_PLATFORM) || defined(CONFIG_APOLLO3_FPGA_PLATFORM)
#else
	while (!pll_clk_lock_check(pll->type));
#endif
	pll_gate_ctrl(pll->type, GATE_ENABLE);

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);
	return 0;
}

static void imapx_pll_disable(struct clk_hw *hw)
{
	struct imapx_pll_clk *pll = to_clk_pll(hw);
	unsigned long flags = 0;

	/* Cannot disable apll, because kernel may crash */
	//if (pll->type == APLL)
	if (!strcmp(pll->hw.clk->name,"apll"))
		return;

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);

	pll_gate_ctrl(pll->type, GATE_DISABLE);
	__clk_writel(DISABLE, PLL_ENABLE(pll->type));

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);
}

static unsigned long imapx_pll_recalc_rate(struct clk_hw *hw,
					unsigned long parent_rate)
{
	struct imapx_pll_clk *pll = to_clk_pll(hw);
	uint32_t clk_sel;

	if(strcmp(pll->hw.clk->name,"apll") && strcmp(pll->hw.clk->name,"dpll")
			&& strcmp(pll->hw.clk->name,"epll") && strcmp(pll->hw.clk->name,"vpll")){
		/* pll err: infotm pll only support APLL, DPLL, EPLL, VPLL*/
		clk_err("Pll only support APLL, DPLL, EPLL, VPLL\n");
		return parent_rate;
	}

	clk_sel = __clk_readl(PLL_SRC_STATUS(pll->type));
	if ((clk_sel & PLL_OUTPUT) == REFERENCE_CLK)
		return parent_rate;

	read_pll_para(pll->params, pll->type);

	return pll_para_to_rate(pll->params, parent_rate);
}

static long imapx_pll_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *prate)
{
	struct imapx_pll_clk *pll = to_clk_pll(hw);
	struct pll_clk_info *info = pll->params;
	unsigned long fvco;

	if (rate >= FOUT_CLK_MAX || rate <= FOUT_CLK_MIN) {
		clk_err("Clk range [36M, 2000M]");
		return -EINVAL;
	}

	info->bypass = 0;
	info->r = *prate / FREF_CLK_MIN - 1;
	info->od = -1;
	fvco = FVCO_CLK_MAX / rate;
	while (fvco) {
		fvco = fvco >> 1;
		info->od += 1;
	}
	if (info->od > 3)
		info->od = 3;
	info->f = rate * (1 << info->od) / (*prate * 2) - 1;

	return pll_para_to_rate(info, *prate);
}

static int imapx_pll_set_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long parent_rate)
{
	struct imapx_pll_clk *pll = to_clk_pll(hw);
	struct pll_clk_info *info = pll->params;
	unsigned long flags = 0;
	uint32_t para, i;

	if (pll->lock)
		spin_lock_irqsave(pll->lock, flags);
	/* unload */
	pll_load_para_control(pll->type, UNLOAD);
	/* set para */
	para = pll_para_to_integr(info->bypass, info->od, info->r, info->f);
	pll_parameter_set(pll->type, para & 0xff, para >> 8);
	/* load */
//	if (pll->type == APLL)
	if (!strcmp(pll->hw.clk->name,"apll"))
		apll_reset(para);
	else {
		pll_output_ctrl(pll->type, PLL_OUTDISABLE);
		__clk_writel(0x2, PLL_SRC_STATUS(pll->type));
		pll_load_para_control(pll->type, LOAD);
		pll_output_ctrl(pll->type, PLL_OUTENABLE);
		for (i = 0; (i < 6666) &&
				!(pll_clk_lock_check(pll->type) & 0x1); i++)
			;
		pll_gate_ctrl(pll->type, GATE_ENABLE);
	}

	if (pll->lock)
		spin_unlock_irqrestore(pll->lock, flags);

	return 0;
}

const struct clk_ops pll_ops = {
	.is_enabled = imapx_pll_is_enabled,
	.enable	= imapx_pll_enable,
	.disable = imapx_pll_disable,
	.recalc_rate = imapx_pll_recalc_rate,
	.round_rate = imapx_pll_round_rate,
	.set_rate = imapx_pll_set_rate,
};

struct clk *imapx_pll_clk_register(const char *name,
				const char *parent_name, enum pll_type type,
				unsigned long flags, spinlock_t *lock)
{
	struct imapx_pll_clk *pll;
	struct clk_init_data init;
	struct pll_clk_info *params;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);
	params = kzalloc(sizeof(*params), GFP_KERNEL);
	if (!params)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.flags = flags;
	init.ops = &pll_ops;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = (parent_name ? 1 : 0);

	pll->params = params;
	pll->type = type;
	pll->lock = lock;
	pll->hw.init = &init;

	return clk_register(NULL, &pll->hw);
}
