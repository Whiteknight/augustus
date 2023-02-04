#include "criteria.h"

#include "scenario/data.h"
#include "building/count.h"
#include "core/log.h"
#include "city/finance.h"
#include "city/population.h"
#include "city/ratings.h"
#include "game/time.h"

static int max_game_year;

win_criteria_t *scenario_criteria_get_ptr(scenario_win_criteria* scenario_ptr, win_criteria_type type)
{
    int first_empty_index = -1;
    int last_disabled_index = -1;
    for (int i = 0; i < MAX_WIN_CRITERIA; i++) {
        if (scenario_ptr->goals[i].type == type) {
            return &scenario_ptr->goals[i];
        }
        if (!scenario_ptr->goals[i].enabled) {
            last_disabled_index = i;
        }
        if (scenario_ptr->goals[i].type == WIN_CRITERIA_NONE && first_empty_index == -1) {
            first_empty_index = i;
        }
    }
    if (first_empty_index >= 0) {
        scenario_ptr->goals[first_empty_index].type = type;
        return &scenario_ptr->goals[first_empty_index];
    }
    if (last_disabled_index >= 0) {
        scenario_ptr->goals[last_disabled_index].type = type;
        return &scenario_ptr->goals[last_disabled_index];
    }
    return NULL;
}

static int has_criteria_enabled(win_criteria_type type)
{
    for (int i = 0; i < MAX_WIN_CRITERIA; i++) {
        if (scenario.win_criteria.goals[i].type == type) {
            return scenario.win_criteria.goals[i].enabled;
        }
    }
    return 0;
}

static int get_win_criteria_value(win_criteria_type type)
{
    for (int i = 0; i < MAX_WIN_CRITERIA; i++) {
        if (scenario.win_criteria.goals[i].type == type && scenario.win_criteria.goals[i].enabled) {
            return scenario.win_criteria.goals[i].goal;
        }
    }
    return 0;
}

void scenario_criteria_clear_ptr(scenario_win_criteria *ptr)
{
    // Clear the criteria data
    for (int i = 0; i < MAX_WIN_CRITERIA; i++) {
        ptr->goals[i].type = WIN_CRITERIA_NONE;
        ptr->goals[i].enabled = 0;
        ptr->goals[i].goal = 0;
        ptr->goals[i].data = 0;
    }

    // allocate a few slots to common types, just so when we try_add later we can find them quick
    ptr->goals[0].type = WIN_CRITERIA_CULTURE;
    ptr->goals[1].type = WIN_CRITERIA_PROSPERITY;
    ptr->goals[2].type = WIN_CRITERIA_PEACE;
    ptr->goals[3].type = WIN_CRITERIA_FAVOR;
    ptr->goals[4].type = WIN_CRITERIA_POPULATION_MINIMUM;
    ptr->goals[5].type = WIN_CRITERIA_TIME_LIMIT;
    ptr->goals[6].type = WIN_CRITERIA_SURVIVAL_YEARS;
}

void scenario_criteria_clear()
{
    scenario_criteria_clear_ptr(&scenario.win_criteria);
}

int scenario_criteria_try_add_or_update_ptr(scenario_win_criteria *ptr, win_criteria_type type, int goal, int data)
{
// Try to find an existing entry with that type. If found, update in-place
    int first_empty_index = -1;
    int last_disabled_index = -1;
    for (int i = 0; i < MAX_WIN_CRITERIA; i++) {
        if (!ptr->goals[i].enabled) {
            last_disabled_index = i;
        }
        if (ptr->goals[i].type == WIN_CRITERIA_NONE && first_empty_index == -1)
        {
            first_empty_index = i;
        }

        if (ptr->goals[i].type == type)
        {
            ptr->goals[i].enabled = 1;
            ptr->goals[i].goal = goal;
            ptr->goals[i].data = data;
            return 1;
        }
    }

    // if we couldn't find an existing entry fill the first empty slot, if we have one.
    if (first_empty_index >= 0)
    {
        ptr->goals[first_empty_index].enabled = 1;
        ptr->goals[first_empty_index].type = type;
        ptr->goals[first_empty_index].goal = goal;
        ptr->goals[first_empty_index].data = data;
        return 1;
    }

    // otherwise we're overwriting a disabled slot. Overwrite the last disabled slot, so we can try not 
    // to overwrite the first couple common ones (culture, prosperity, peace, favor, etc)
    if (last_disabled_index >= 0)
    {
        ptr->goals[last_disabled_index].enabled = 1;
        ptr->goals[last_disabled_index].type = type;
        ptr->goals[last_disabled_index].goal = goal;
        ptr->goals[last_disabled_index].data = data;
        return 1;
    }

    return 0;
}

// Enable the given criteria type or, if it's already enabled, update the values
int scenario_criteria_try_add_or_update(win_criteria_type type, int goal, int data)
{
    scenario_criteria_try_add_or_update_ptr(&scenario.win_criteria, type, goal, data);
}

// Try to toggle an existing slot, with existing data.
// If the slot does not exist, create one with default values
// returns the current enabled state of the criteria
int scenario_criteria_toggle(win_criteria_type type, int defaultGoal, int defaultData)
{
    // If we have an existing slot just toggle the enable flag and leave.
    for (int i = 0; i < MAX_WIN_CRITERIA; i++) {
        if (scenario.win_criteria.goals[i].type == type) {
            scenario.win_criteria.goals[i].enabled = !scenario.win_criteria.goals[i].enabled;
            return scenario.win_criteria.goals[i].enabled;
        }
    }

    // Otherwise it doesn't exist (disabled) so we add it.
    return scenario_criteria_try_add_or_update(type, defaultGoal, defaultData);
}

int scenario_criteria_disable(win_criteria_type type)
{
    // Don't bail out early just in case we somehow get a duplicate entry
    for (int i = 0; i < MAX_WIN_CRITERIA; i++) {
        if (scenario.win_criteria.goals[i].type == type) {
            scenario.win_criteria.goals[i].enabled = 0;
        }
    }
}

static win_criteria_satisfy_state test_criteria(win_criteria_type type, int goal, int data)
{
    switch (type) {
        case WIN_CRITERIA_POPULATION_MINIMUM:
            return city_population() >= goal ? WIN_CRITERIA_STATE_OK : WIN_CRITERIA_STATE_FAIL;
        case WIN_CRITERIA_CULTURE:
            return city_rating_culture() >= goal ? WIN_CRITERIA_STATE_OK : WIN_CRITERIA_STATE_FAIL;
        case WIN_CRITERIA_PROSPERITY:
            return city_rating_prosperity() >= goal ? WIN_CRITERIA_STATE_OK : WIN_CRITERIA_STATE_FAIL;
        case WIN_CRITERIA_PEACE:
            return city_rating_peace() >= goal ? WIN_CRITERIA_STATE_OK : WIN_CRITERIA_STATE_FAIL;
        case WIN_CRITERIA_FAVOR:
            return city_rating_favor() >= goal ? WIN_CRITERIA_STATE_OK : WIN_CRITERIA_STATE_FAIL;
        case WIN_CRITERIA_SURVIVAL_YEARS:
            if (game_time_year() > scenario.start_year + goal)
                return WIN_CRITERIA_STATE_OK;
            return WIN_CRITERIA_STATE_LOSE;
        case WIN_CRITERIA_TIME_LIMIT:
            if (game_time_year() <= scenario.start_year + goal)
                return WIN_CRITERIA_STATE_OK;
            return WIN_CRITERIA_STATE_FAIL;
        case WIN_CRITERIA_EVOLVE_X_HOUSES:
            return building_count_active(BUILDING_HOUSE_SMALL_TENT + data) >= goal ? WIN_CRITERIA_STATE_OK : WIN_CRITERIA_STATE_FAIL;
        case WIN_CRITERIA_BUILD_X_BUILDINGS:
            return building_count_active(data) >= goal ? WIN_CRITERIA_STATE_OK : WIN_CRITERIA_STATE_FAIL;
        case WIN_CRITERIA_EARN_X_DENARII:
            return city_finance_treasury() >= goal ? WIN_CRITERIA_STATE_OK : WIN_CRITERIA_STATE_FAIL;
    }

    // The criteria type is unsupported, so just return failure. 
    log_error("Unknown win criteria type tested", 0, type);
    return WIN_CRITERIA_STATE_FAIL;
}

win_criteria_satisfy_state scenario_criteria_test_all()
{
    // Check all criteria for victory. 
    // If there are no criteria, return NONE (free play)
    // If any criteria LOSE, return LOSE (game over)
    // if any criteria FAIL, return FAIL (keep playing)
    // if all criteria WIN, return WIN (accept promotion)
    int num_criteria = 0;
    for (int i = 0; i < MAX_WIN_CRITERIA; i++) {
        if (!scenario.win_criteria.goals[i].enabled) {
            continue;
        }

        num_criteria++;
        win_criteria_type type = scenario.win_criteria.goals[i].type;
        int goal = scenario.win_criteria.goals[i].goal;
        int data = scenario.win_criteria.goals[i].data;
        win_criteria_satisfy_state state = test_criteria(type, goal, data);
        if (state == WIN_CRITERIA_STATE_LOSE || state == WIN_CRITERIA_STATE_FAIL) {
            return state;
        }
    }

    return num_criteria > 0 ? WIN_CRITERIA_STATE_OK : WIN_CRITERIA_STATE_NONE;
}

int scenario_criteria_population_enabled(void)
{
    return has_criteria_enabled(WIN_CRITERIA_POPULATION_MINIMUM);
}

int scenario_criteria_population(void)
{
    return get_win_criteria_value(WIN_CRITERIA_POPULATION_MINIMUM);
}

int scenario_criteria_culture_enabled(void)
{
    return has_criteria_enabled(WIN_CRITERIA_CULTURE);
}

int scenario_criteria_culture(void)
{
    return get_win_criteria_value(WIN_CRITERIA_CULTURE);
}

int scenario_criteria_prosperity_enabled(void)
{
    return has_criteria_enabled(WIN_CRITERIA_PROSPERITY);
}

int scenario_criteria_prosperity(void)
{
    return get_win_criteria_value(WIN_CRITERIA_PROSPERITY);
}

int scenario_criteria_peace_enabled(void)
{
    return has_criteria_enabled(WIN_CRITERIA_PEACE);
}

int scenario_criteria_peace(void)
{
    return get_win_criteria_value(WIN_CRITERIA_PEACE);
}

int scenario_criteria_favor_enabled(void)
{
    return has_criteria_enabled(WIN_CRITERIA_FAVOR);
}

int scenario_criteria_favor(void)
{
    return has_criteria_enabled(WIN_CRITERIA_FAVOR);
}

int scenario_criteria_time_limit_enabled(void)
{
    return has_criteria_enabled(WIN_CRITERIA_TIME_LIMIT);
}

int scenario_criteria_time_limit_years(void)
{
    return get_win_criteria_value(WIN_CRITERIA_TIME_LIMIT);
}

int scenario_criteria_survival_enabled(void)
{
    return has_criteria_enabled(WIN_CRITERIA_SURVIVAL_YEARS);
}

int scenario_criteria_survival_years(void)
{
    return has_criteria_enabled(WIN_CRITERIA_SURVIVAL_YEARS);
}

int scenario_criteria_milestone_year(int percentage)
{
    switch (percentage) {
        case 25:
            return scenario.start_year + scenario.win_criteria.milestone25_year;
        case 50:
            return scenario.start_year + scenario.win_criteria.milestone50_year;
        case 75:
            return scenario.start_year + scenario.win_criteria.milestone75_year;
        default:
            return 0;
    }
}

void scenario_criteria_init_max_year(void)
{
    if (scenario_criteria_time_limit_enabled()) {
        max_game_year = scenario.start_year + scenario_criteria_time_limit_years();
    } else if (scenario_criteria_survival_enabled()) {
        max_game_year = scenario.start_year + scenario_criteria_survival_years();
    } else {
        max_game_year = 1000000 + scenario.start_year;
    }
}

int scenario_criteria_max_year(void)
{
    return max_game_year;
}

void scenario_criteria_save_state(buffer *buf)
{
    buffer_write_i32(buf, max_game_year);
}

void scenario_criteria_load_state(buffer *buf)
{
    max_game_year = buffer_read_i32(buf);
}
