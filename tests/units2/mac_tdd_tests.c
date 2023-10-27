// This file contains unit tests for MAC (Medium Access Control) and TDD (Time Division Duplexing) functions.

/*
Good to know:

A cycle consists of:
    My window
    Gap
    Peer window
    Tail
Then the cycle resets.
*/

#include "units.h"

static int get_cycle(SkyMAC* mac){
	return mac->my_window_length + mac->config->gap_constant_ticks + mac->peer_window_length + mac->config->tail_constant_ticks;
}

// Test Creating a MAC instance.
TEST(mac_create){
    // Default config.
    SkyConfig* config = malloc(sizeof(SkyConfig));
    default_config(config);
    // Create SkyHandle struct.
    SkyHandle handle = sky_create(config);
    // Create SkyMAC struct.
    SkyMAC* mac = sky_mac_create(&handle->conf->mac);

    // Check that the struct is not NULL.
    ASSERT(mac != NULL, "Create MAC failed.");
    // Assert values of the config.
    ASSERT(mac->config->maximum_window_length_ticks == 1000, "MAC window length ticks should be 0, was: %d", mac->config->maximum_window_length_ticks);
    ASSERT(mac->config->minimum_window_length_ticks == 250, "MAC window length ticks should be 1, was: %d", mac->config->minimum_window_length_ticks);
    ASSERT(mac->config->idle_frames_per_window == 0, "MAC idle frames per window should be 0, was: %d", mac->config->idle_frames_per_window);
    ASSERT(mac->config->idle_timeout_ticks == 30000, "MAC idle timeout ticks should be 30000, was: %d", mac->config->idle_timeout_ticks);
    ASSERT(mac->config->carrier_sense_ticks == 200, "MAC carrier sense ticks should be 200, was: %d", mac->config->carrier_sense_ticks);
    ASSERT(mac->config->gap_constant_ticks == 600, "MAC gap constant ticks should be 600, was: %d", mac->config->gap_constant_ticks);
    ASSERT(mac->config->tail_constant_ticks == 80, "MAC tail constant ticks should be 80, was: %d", mac->config->tail_constant_ticks);
    ASSERT(mac->config->window_adjust_increment_ticks == 250, "MAC window adjust increment ticks should be 250, was: %d", mac->config->window_adjust_increment_ticks);
    ASSERT(mac->config->window_adjustment_period == 2, "MAC window adjust period should be 2, was: %d", mac->config->window_adjustment_period);
    ASSERT(mac->config->unauthenticated_mac_updates == 0, "MAC unauthenticated MAC updates should be 0, was: %d", mac->config->unauthenticated_mac_updates);
    ASSERT(mac->config->shift_threshold_ticks == 10000, "MAC shift threshold ticks should be 10000, was: %d", mac->config->shift_threshold_ticks);
    
    // Assert values of the mac.
    ASSERT(mac->T0 == 0, "MAC T0 should be 0, was: %d", mac->T0);
    ASSERT(mac->window_on == false, "MAC window on should be false, was: %d", mac->window_on);
    ASSERT(mac->unused_window_time == 0, "MAC unused window time should be 0, was: %d", mac->unused_window_time);
    ASSERT(mac->my_window_length == mac->config->minimum_window_length_ticks, "MAC my window length should be %d, was: %d", mac->config->minimum_window_length_ticks, mac->my_window_length);
    ASSERT(mac->peer_window_length == mac->config->minimum_window_length_ticks, "MAC peer window length should be %d, was: %d", mac->config->minimum_window_length_ticks, mac->peer_window_length);
    ASSERT(mac->window_adjust_counter == 0, "MAC window adjust counter should be 0, was: %d", mac->window_adjust_counter);
    ASSERT(mac->vc_round_robin_start == 0, "MAC vc round robin start should be 0, was: %d", mac->vc_round_robin_start);
    ASSERT(mac->last_belief_update == (MOD_TIME_TICKS - 300000), "MAC last belief update should be %d, was: %d", MOD_TIME_TICKS - 300000, mac->last_belief_update);
    for(int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++){
        ASSERT(mac->frames_sent_in_current_window_per_vc[i] == 0, "MAC frames sent in current window per vc should be 0, was: %d for vc: %d", mac->frames_sent_in_current_window_per_vc[i], i);
    }
    ASSERT(mac->total_frames_sent_in_current_window == 0, "MAC total frames sent in current window should be 0, was: %d", mac->total_frames_sent_in_current_window);
    // Destroy the handle.
    sky_destroy(handle);
    // Free the config.
    SKY_FREE(config);
    // Destroy the mac.
    sky_mac_destroy(mac);

}
// Should default to certain values with invalid config.
TEST(mac_create_invalid_config){
    // Create the first config, this test will create two mac instances in order to check all invalid values.
    SkyMACConfig* config = malloc(sizeof(SkyMACConfig));
    // Set invalid values.
    // Min window length ticks > Max window length ticks should default max to min.
    config->maximum_window_length_ticks = 100;
    config->minimum_window_length_ticks = 200;
    // Idle franes per window > 3 should default to 3
    config->idle_frames_per_window = 4;
    // Idle timeout ticks < 5000 or > 90000 should default to 25000
    config->idle_timeout_ticks = 4999;
    // Carrier sense ticks >= (config->minimum_window_length_ticks + config->gap_constant_ticks) results in carrier sense ticks = (config->minimum_window_length_ticks + config->gap_constant_ticks).
    config->carrier_sense_ticks = 1000;
    // Window adjustment period < 1 should result in 1 and > 4 should result in 4.
    config->window_adjustment_period = 0;
    // Remaining values have no checks so set defaults.
    config->gap_constant_ticks = 600;
    config->tail_constant_ticks = 80;
    config->window_adjust_increment_ticks = 250;
    config->unauthenticated_mac_updates = 0;
    config->shift_threshold_ticks = 10000;
    // Create SkyMAC struct.
    SkyMAC* mac = sky_mac_create(config);

    // Assert values of the config, should change to defaults.
    ASSERT(mac->config->maximum_window_length_ticks == 200, "MAC window length ticks should be 200, was: %d", mac->config->maximum_window_length_ticks);
    ASSERT(mac->config->minimum_window_length_ticks == 200, "MAC window length ticks should be 200, was: %d", mac->config->minimum_window_length_ticks);
    ASSERT(mac->config->idle_frames_per_window == 3, "MAC idle frames per window should be 3, was: %d", mac->config->idle_frames_per_window);
    ASSERT(mac->config->idle_timeout_ticks == 25000, "MAC idle timeout ticks should be 25000, was: %d", mac->config->idle_timeout_ticks);
    ASSERT(mac->config->carrier_sense_ticks == 800, "MAC carrier sense ticks should be 800, was: %d", mac->config->carrier_sense_ticks);
    ASSERT(mac->config->gap_constant_ticks == 600, "MAC gap constant ticks should be 600, was: %d", mac->config->gap_constant_ticks);
    ASSERT(mac->config->tail_constant_ticks == 80, "MAC tail constant ticks should be 80, was: %d", mac->config->tail_constant_ticks);
    ASSERT(mac->config->window_adjust_increment_ticks == 250, "MAC window adjust increment ticks should be 250, was: %d", mac->config->window_adjust_increment_ticks);
    ASSERT(mac->config->window_adjustment_period == 1, "MAC window adjust period should be 1, was: %d", mac->config->window_adjustment_period);
    ASSERT(mac->config->unauthenticated_mac_updates == 0, "MAC unauthenticated MAC updates should be 0, was: %d", mac->config->unauthenticated_mac_updates);
    ASSERT(mac->config->shift_threshold_ticks == 10000, "MAC shift threshold ticks should be 10000, was: %d", mac->config->shift_threshold_ticks);

    // Destroy the mac.
    sky_mac_destroy(mac);
    // Free the config.
    SKY_FREE(config);

    // Create the second config.
    config = malloc(sizeof(SkyMACConfig));
    // Set invalid values.
    // Min window length ticks > Max window length ticks should default max to min.
    config->maximum_window_length_ticks = 200;
    config->minimum_window_length_ticks = 800;
    // Idle franes per window > 3 should default to 3
    config->idle_frames_per_window = 4;
    // Idle timeout ticks < 5000 or > 90000 should default to 25000
    config->idle_timeout_ticks = 90001;
    // Carrier sense ticks >= (config->minimum_window_length_ticks + config->gap_constant_ticks) results in carrier sense ticks = (config->minimum_window_length_ticks + config->gap_constant_ticks).
    config->carrier_sense_ticks = 2000;
    // Window adjustment period < 1 should result in 1 and > 4 should result in 4.
    config->window_adjustment_period = 5;
    // Remaining values have no checks so set defaults.
    config->gap_constant_ticks = 600;
    config->tail_constant_ticks = 80;
    config->window_adjust_increment_ticks = 250;
    config->unauthenticated_mac_updates = 0;
    config->shift_threshold_ticks = 10000;
    // Create SkyMAC struct.
    mac = sky_mac_create(config);

    // Assert values of the config, should change to defaults.
    ASSERT(mac->config->maximum_window_length_ticks == 800, "MAC window length ticks should be 100, was: %d", mac->config->maximum_window_length_ticks);
    ASSERT(mac->config->minimum_window_length_ticks == 800, "MAC window length ticks should be 100, was: %d", mac->config->minimum_window_length_ticks);
    ASSERT(mac->config->idle_frames_per_window == 3, "MAC idle frames per window should be 3, was: %d", mac->config->idle_frames_per_window);
    ASSERT(mac->config->idle_timeout_ticks == 25000, "MAC idle timeout ticks should be 25000, was: %d", mac->config->idle_timeout_ticks);
    ASSERT(mac->config->carrier_sense_ticks == 1400, "MAC carrier sense ticks should be 1400, was: %d", mac->config->carrier_sense_ticks);
    ASSERT(mac->config->gap_constant_ticks == 600, "MAC gap constant ticks should be 600, was: %d", mac->config->gap_constant_ticks);
    ASSERT(mac->config->tail_constant_ticks == 80, "MAC tail constant ticks should be 80, was: %d", mac->config->tail_constant_ticks);
    ASSERT(mac->config->window_adjust_increment_ticks == 250, "MAC window adjust increment ticks should be 250, was: %d", mac->config->window_adjust_increment_ticks);
    ASSERT(mac->config->window_adjustment_period == 4, "MAC window adjust period should be 4, was: %d", mac->config->window_adjustment_period);
    ASSERT(mac->config->unauthenticated_mac_updates == 0, "MAC unauthenticated MAC updates should be 0, was: %d", mac->config->unauthenticated_mac_updates);
    ASSERT(mac->config->shift_threshold_ticks == 10000, "MAC shift threshold ticks should be 10000, was: %d", mac->config->shift_threshold_ticks);

    // Destroy the mac.
    sky_mac_destroy(mac);
    // Free the config.
    SKY_FREE(config);
}

// Test mac window shift.
// If called after creation of MAC, T0 is 0 and shift is wrapped around since shift is subtracted from T0. Is this intended? Start of cycle will move to 2^24 - shift...
TEST(mac_window_shift){
    // Default config.
    SkyConfig* config = malloc(sizeof(SkyConfig));
    default_config(config);
    // Create SkyHandle struct.
    SkyHandle handle = sky_create(config);
    // Create SkyMAC struct.
    SkyMAC* mac = sky_mac_create(&handle->conf->mac);

    // Tests:
    // Shift positive and negative lengths. Should shift backward by same amount no matter the sign.
    mac->T0 = 2000;
    // Shift by 250.
    mac_shift_windowing(mac, 250);
    // Check T0 == 250.
    ASSERT(mac->T0 == 1750, "MAC T0 should be 250, was: %d", mac->T0);
    // Shift by -250.
    mac_shift_windowing(mac, -250);
    // Check T0 == 500.
    ASSERT(mac->T0 == 1500, "MAC T0 should be 500, was: %d", mac->T0);
    // Shift by 0.
    mac_shift_windowing(mac, 0);
    // Check T0 == 500.
    ASSERT(mac->T0 == 1500, "MAC T0 should be 500, was: %d", mac->T0);
    // Shift by -500.
    mac_shift_windowing(mac, -500);
    // Check T0 == 1000.
    ASSERT(mac->T0 == 1000, "MAC T0 should be 1000, was: %d", mac->T0);
    // Destroy the handle.
    sky_destroy(handle);
    // Free the config.
    SKY_FREE(config);
    // Destroy the mac.
    sky_mac_destroy(mac);

}

// Test mac window shift wrap around. Wraps around MOD_TIME_TICKS = 2^24.
TEST(mac_window_shift_wrap_around){
    // Default config.
    SkyConfig* config = malloc(sizeof(SkyConfig));
    default_config(config);
    // Create SkyHandle struct.
    SkyHandle handle = sky_create(config);
    // Create SkyMAC struct.
    SkyMAC* mac = sky_mac_create(&handle->conf->mac);

    // Tests:
    // Check that T0 is 0.
    ASSERT(mac->T0 == 0, "MAC T0 should be 0, was: %d", mac->T0);
    // Shift by 2^24 - 1.
    mac_shift_windowing(mac, MOD_TIME_TICKS - 1);
    // Check T0 == 1.
    ASSERT(mac->T0 == 1, "MAC T0 should be 1, was: %d", mac->T0);
    // Shift by 2.
    mac_shift_windowing(mac, -2);
    // Check T0 == 2^24 - 1.
    ASSERT(mac->T0 == MOD_TIME_TICKS - 1, "MAC T0 should be %d, was: %d", MOD_TIME_TICKS - 1, mac->T0);
    // Shift by 1.
    mac_shift_windowing(mac, 1);
    // Check T0 == MOD_TIME_TICKS - 2.
    ASSERT(mac->T0 == MOD_TIME_TICKS - 2, "MAC T0 should be %d, was: %d", MOD_TIME_TICKS - 2, mac->T0);
    // Shift by 2^24.
    mac_shift_windowing(mac, MOD_TIME_TICKS);
    // Check T0 == MOD_TIME_TICKS - 2.
    ASSERT(mac->T0 == MOD_TIME_TICKS - 2, "MAC T0 should be %d, was: %d", MOD_TIME_TICKS - 2, mac->T0);
    // Shift by -2^24.
    mac_shift_windowing(mac, -MOD_TIME_TICKS);
    // Check T0 == MOD_TIME_TICKS - 2.
    ASSERT(mac->T0 == MOD_TIME_TICKS - 2, "MAC T0 should be %d, was: %d", MOD_TIME_TICKS - 2, mac->T0);

    // Destroy the handle.
    sky_destroy(handle);
    // Free the config.
    SKY_FREE(config);
    // Destroy the mac.
    sky_mac_destroy(mac);
}
// Test expanding window.
TEST(mac_window_expand){
    // Default config.
    SkyConfig* config = malloc(sizeof(SkyConfig));
    default_config(config);
    // Create SkyHandle struct.
    SkyHandle handle = sky_create(config);
    // Create SkyMAC struct.
    SkyMAC* mac = sky_mac_create(&handle->conf->mac);

    // Tests:
    // Check my window length and T0.
    ASSERT(mac->my_window_length == mac->config->minimum_window_length_ticks, "MAC my window length should be %d, was: %d", mac->config->minimum_window_length_ticks, mac->my_window_length);
    ASSERT(mac->T0 == 0, "MAC T0 should be 0, was: %d", mac->T0);
    // Expand window, should expand by window adjust increment ticks, second argument is the tick for current time, not the expansion.
    mac_expand_window(mac, 3000);
    // Check my window length and T0.
    ASSERT(mac->my_window_length == mac->config->minimum_window_length_ticks + 250, "MAC my window length should be %d, was: %d", mac->config->minimum_window_length_ticks + 250, mac->my_window_length);
    ASSERT(mac->T0 == 3000 - get_cycle(mac), "MAC T0 should be %d, was: %d", 3000 - get_cycle(mac), mac->T0);
    // Repeat expansion and checks until max window length is reached.
    mac_expand_window(mac, 6000);
    ASSERT(mac->my_window_length == mac->config->minimum_window_length_ticks + 500, "MAC my window length should be %d, was: %d", mac->config->minimum_window_length_ticks + 500, mac->my_window_length);
    ASSERT(mac->T0 == 6000 - get_cycle(mac), "MAC T0 should be %d, was: %d", 6000 - get_cycle(mac), mac->T0);
    mac_expand_window(mac, 9000);
    ASSERT(mac->my_window_length == mac->config->minimum_window_length_ticks + 750, "MAC my window length should be %d, was: %d", mac->config->minimum_window_length_ticks + 750, mac->my_window_length);
    ASSERT(mac->T0 == 9000 - get_cycle(mac), "MAC T0 should be %d, was: %d", 9000 - get_cycle(mac), mac->T0);
    // Max window length reached, should not expand.
    mac_expand_window(mac, 12000);
    ASSERT(mac->my_window_length == mac->config->minimum_window_length_ticks + 750, "MAC my window length should be %d, was: %d", mac->config->minimum_window_length_ticks + 750, mac->my_window_length);
    ASSERT(mac->T0 == 12000 - get_cycle(mac), "MAC T0 should be %d, was: %d", 12000 - get_cycle(mac), mac->T0);
    // Destroy the handle.
    sky_destroy(handle);
    // Free the config.
    SKY_FREE(config);
    // Destroy the mac.
    sky_mac_destroy(mac);
}
// MAC CYCLE LENGTH:
// mac->my_window_length + mac->config->gap_constant_ticks + mac->peer_window_length + mac->config->tail_constant_ticks;
// Test shrinking window.
TEST(mac_window_shrink){
    // Default config.
    SkyConfig* config = malloc(sizeof(SkyConfig));
    default_config(config);
    config->mac.window_adjust_increment_ticks = 300;
    // Create SkyHandle struct.
    SkyHandle handle = sky_create(config);
    // Create SkyMAC struct.
    SkyMAC* mac = sky_mac_create(&handle->conf->mac);

    // Tests:
    // Set my window length to max.
    mac->my_window_length = mac->config->maximum_window_length_ticks;
    // Check my window length, window adjust increment ticks and T0.
    ASSERT(mac->my_window_length == mac->config->maximum_window_length_ticks, "MAC my window length should be %d, was: %d", mac->config->maximum_window_length_ticks, mac->my_window_length);
    ASSERT(mac->config->window_adjust_increment_ticks == 300, "MAC window adjust increment ticks should be 300, was: %d", mac->config->window_adjust_increment_ticks);
    ASSERT(mac->T0 == 0, "MAC T0 should be 0, was: %d", mac->T0);

    // Shrink window, should shrink by window adjust increment ticks, second argument is the tick for current time, not the shrink.
    mac_shrink_window(mac, 0);
    // Check my window length and T0.
    ASSERT(mac->my_window_length == mac->config->maximum_window_length_ticks - 300, "MAC my window length should be %d, was: %d", mac->config->maximum_window_length_ticks - 300, mac->my_window_length);
    ASSERT(mac->T0 == MOD_TIME_TICKS - get_cycle(mac), "MAC T0 should be %d, was: %d", MOD_TIME_TICKS - get_cycle(mac), mac->T0);
    // Repeat shrinking and checks until min window length is reached.
    mac_shrink_window(mac, 100);
    ASSERT(mac->my_window_length == mac->config->maximum_window_length_ticks - 600, "MAC my window length should be %d, was: %d", mac->config->maximum_window_length_ticks - 600, mac->my_window_length);
    ASSERT(mac->T0 == (MOD_TIME_TICKS + 100) - get_cycle(mac), "MAC T0 should be %d, was: %d", (MOD_TIME_TICKS + 100) - get_cycle(mac), mac->T0);
    // Should now be 250. (MIN REACHED)
    mac_shrink_window(mac, 200);
    ASSERT(mac->my_window_length == mac->config->minimum_window_length_ticks, "MAC my window length should be %d, was: %d", mac->config->minimum_window_length_ticks, mac->my_window_length);
    ASSERT(mac->T0 == (MOD_TIME_TICKS + 200) - get_cycle(mac), "MAC T0 should be %d, was: %d", (MOD_TIME_TICKS + 200) - get_cycle(mac), mac->T0);
    // Destroy the handle.
    sky_destroy(handle);
    // Free the config.
    SKY_FREE(config);
    // Destroy the mac.
    sky_mac_destroy(mac);
}

// Time to window/Window remaining/Peer window remaining/Can send
TEST(mac_window_times){
    // Default config.
    SkyConfig* config = malloc(sizeof(SkyConfig));
    default_config(config);
    // Create SkyHandle struct.
    SkyHandle handle = sky_create(config);
    // Create SkyMAC struct.
    SkyMAC* mac = sky_mac_create(&handle->conf->mac);
    int n = 10000;
    int increment = 1;
    int now = 0;
    int cycle_length = get_cycle(mac);
    /*
    Test that all the times are correct for different 'now' timestamps. Loop through values of now 'n' times and increment 'now' by 'increment' each time.
    These times are Time to window, Window remaining and Peer window remaining.
    If in own window, can send should be true.
    */
    for(int i = 0; i < n; i++){
        // Set now.
        now += increment;
        int time_in_cycle = positive_modulo(now,cycle_length);
        // Set window on.
        // Check if in own window, gap, peer window or tail.
        printf("%d ", i);
        if(time_in_cycle < mac->my_window_length){ // OWN WINDOW
            ASSERT(mac_time_to_own_window(mac,now) == 0, "OWN WINDOW: Time to own window should be 0, was: %d", mac_time_to_own_window(mac,now));
            ASSERT(mac_own_window_remaining(mac, now) == (mac->my_window_length - time_in_cycle), "OWN WINDOW: Own window remaining should be %d, was: %d", (mac->my_window_length - time_in_cycle), mac_own_window_remaining(mac, now));
            ASSERT(mac_peer_window_remaining(mac, now) < 0, "OWN WINDOW: Peer window remaining should be less than 0, was: %d", mac_peer_window_remaining(mac, now));
            ASSERT(mac_can_send(mac, now) == true, "OWN WINDOW: Can send should be true, was: %d", mac_can_send(mac, now));
        }
        else if(time_in_cycle < mac->my_window_length + mac->config->gap_constant_ticks){ // GAP
            ASSERT(mac_time_to_own_window(mac,now) == (cycle_length - time_in_cycle), "GAP: Time to own window should be %d, was: %d", (cycle_length - time_in_cycle), mac_time_to_own_window(mac,now));
            ASSERT(mac_own_window_remaining(mac, now) == (mac->my_window_length - time_in_cycle), "GAP: Own window remaining should be %d, was: %d", (mac->my_window_length - time_in_cycle), mac_own_window_remaining(mac, now));
            ASSERT(mac_peer_window_remaining(mac, now) < 0, "GAP: Peer window remaining should be less than 0, was: %d", mac_peer_window_remaining(mac, now));
            ASSERT(mac_can_send(mac, now) == false, "GAP: Can send should be false, was: %d", mac_can_send(mac, now));
        }
        else if(time_in_cycle < mac->my_window_length + mac->config->gap_constant_ticks + mac->peer_window_length){ // Peer window
        ASSERT(mac_time_to_own_window(mac,now) == (cycle_length - time_in_cycle), "PEER WINDOW: Time to own window should be %d, was: %d", (time_in_cycle - mac->my_window_length), mac_time_to_own_window(mac,now));
        ASSERT(mac_own_window_remaining(mac, now) == (mac->my_window_length - time_in_cycle), "PEER WINDOW: Own window remaining should be 0, was: %d", mac_own_window_remaining(mac, now));
        ASSERT(mac_peer_window_remaining(mac, now) == mac->peer_window_length + mac->config->gap_constant_ticks + mac->my_window_length - time_in_cycle, "PEER WINDOW: Peer window remaining should be %d, was: %d", mac->peer_window_length + mac->config->gap_constant_ticks + mac->my_window_length - time_in_cycle, mac_peer_window_remaining(mac, now));
        ASSERT(mac_can_send(mac, now) == false, "PEER WINDOW: Can send should be false, was: %d", mac_can_send(mac, now));
        }
        else{ // Tail
        ASSERT(mac_time_to_own_window(mac,now) == (cycle_length - time_in_cycle), "TAIL: Time to own window should be %d, was: %d", (time_in_cycle - mac->my_window_length), mac_time_to_own_window(mac,now));
        ASSERT(mac_own_window_remaining(mac, now) == (mac->my_window_length - time_in_cycle), "TAIL: Own window remaining should be %d, was: %d", (mac->my_window_length - time_in_cycle), mac_own_window_remaining(mac, now));
        ASSERT(mac_peer_window_remaining(mac, now) == (mac->my_window_length + mac->config->gap_constant_ticks + mac->peer_window_length - time_in_cycle), "TAIL: Peer window remaining should be %d, was: %d", (mac->my_window_length + mac->config->gap_constant_ticks + mac->peer_window_length - time_in_cycle), mac_peer_window_remaining(mac, now));
        ASSERT(mac_can_send(mac, now) == false, "TAIL: Can send should be false, was: %d", mac_can_send(mac, now));
        }
        }

    // Destroy the handle.
    sky_destroy(handle);
    // Free the config.
    SKY_FREE(config);
    // Destroy the mac.
    sky_mac_destroy(mac);
}

// Mac reset, set so cant send, reset and check if it can send. Randomized now time to easily test different times.
TEST(mac_reset_to_send){
    // Default config.
    SkyConfig* config = malloc(sizeof(SkyConfig));
    default_config(config);
    // Create SkyHandle struct.
    SkyHandle handle = sky_create(config);
    // Create SkyMAC struct.
    SkyMAC* mac = sky_mac_create(&handle->conf->mac);
    int n = 100000;
    // Tests:
    // Reseting no matter the time should result in mac_can_send(mac, now) == true.
    // n random now resets should all result in being able to send at that time.
    for(int i = 0; i < n; i++){
        mac->window_adjust_counter++; // Little bit useless to test reseting every time.
        int now = rand(); // Random now time. Should extensively test wrapping at the same time since most numbers will be a lot greater than 2^24.
        mac_reset(mac, now);
        ASSERT(mac_can_send(mac, now) == true, "Can send should be true, was: %d", mac_can_send(mac, now));        
        ASSERT(mac->window_adjust_counter == 0, "Window adjust counter should be reset to 0, was: %d", mac->window_adjust_counter);
    }

    // Destroy the handle.
    sky_destroy(handle);
    // Free the config.
    SKY_FREE(config);
    // Destroy the mac.
    sky_mac_destroy(mac);
}
// Test updating belief.
/*
function: mac_update_belief(SkyMAC* mac, const sky_tick_t now, sky_tick_t receive_time, sky_tick_t peer_mac_length, sky_tick_t peer_mac_remaining)
Steps for belief update for easier understanding:
Has parameters: SkyMAC* mac, const sky_tick_t now, sky_tick_t receive_time, sky_tick_t peer_mac_length, sky_tick_t peer_mac_remaining
First check if peer_mac length is between min and max length, if not set to min/max.
Check if peer_mac_remaining is less than peer_mac_length, if so set to peer_mac_length.

Check if now and recieve time are on the same side of the tick modulo.

Update belief update time, and peer length.

Calculate two possible values for T0, one is based on recieve time and the values given by the peer 
and the other is the minimum time based on current time and tail length.
If the peer value is greater than the minimum value, set T0 to the peer value.
If the peer value is less than the minimum value, set T0 to the minimum value.

Reset counters for frames sent in current window per vc and total frames sent in current window.
*/
TEST(mac_belief_update){
    // Default config.
    SkyConfig* config = malloc(sizeof(SkyConfig));
    default_config(config);
    // Create SkyHandle struct.
    SkyHandle handle = sky_create(config);
    // Create SkyMAC struct.
    SkyMAC* mac = sky_mac_create(&handle->conf->mac);

    // Test both versions of T0 calculation.
    // Initial values:
    ASSERT(mac->T0 == 0, "MAC T0 should be 0, was: %d", mac->T0);
    ASSERT(mac->last_belief_update == (MOD_TIME_TICKS - 300000), "MAC last belief update should be %d, was: %d", MOD_TIME_TICKS - 300000, mac->last_belief_update);
    for(int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++){
        ASSERT(mac->frames_sent_in_current_window_per_vc[i] == 0, "MAC frames sent in current window per vc should be 0, was: %d for vc: %d", mac->frames_sent_in_current_window_per_vc[i], i);
    }
    ASSERT(mac->total_frames_sent_in_current_window == 0, "MAC total frames sent in current window should be 0, was: %d", mac->total_frames_sent_in_current_window);
    ASSERT(mac->peer_window_length == mac->config->minimum_window_length_ticks, "MAC peer window length should be %d, was: %d", mac->config->minimum_window_length_ticks, mac->peer_window_length);

    // Tail length is 80.
    // Set values for frames sent in current window per vc and total frames sent in current window to see that they are reset.
    for(int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++){
        mac->frames_sent_in_current_window_per_vc[i] = 2;
    }
    mac->total_frames_sent_in_current_window = 8;
    mac_update_belief(mac, 2010, 2005, 400, 200);
    // Check that values are set correctly. Get cycle should have a different value since peer length is different.
    ASSERT(mac->T0 == 2285-get_cycle(mac), "MAC T0 should be %d, was: %d", 2285-get_cycle(mac) , mac->T0);
    ASSERT(mac->last_belief_update == 2010, "MAC last belief update should be 2010, was: %d", mac->last_belief_update);
    // Assert that values are reset for first test only and assume that the rest of the tests work.
    for(int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++){
        ASSERT(mac->frames_sent_in_current_window_per_vc[i] == 0, "MAC frames sent in current window per vc should be 0, was: %d for vc: %d", mac->frames_sent_in_current_window_per_vc[i], i);
    }
    ASSERT(mac->total_frames_sent_in_current_window == 0, "MAC total frames sent in current window should be 0, was: %d", mac->total_frames_sent_in_current_window);
    ASSERT(mac->peer_window_length == 400, "MAC peer window length should be 400, was: %d", mac->peer_window_length);
    // Update belief again with same values, should not reset values.
    mac_update_belief(mac, 2010, 2005, 400, 200);
    // Check that values are not reset.
    ASSERT(mac->T0 == 2285-get_cycle(mac), "MAC T0 should be %d, was: %d", 2285-get_cycle(mac) , mac->T0);
    ASSERT(mac->last_belief_update == 2010, "MAC last belief update should be 2010, was: %d", mac->last_belief_update);
    for(int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++){
        ASSERT(mac->frames_sent_in_current_window_per_vc[i] == 0, "MAC frames sent in current window per vc should be 0, was: %d for vc: %d", mac->frames_sent_in_current_window_per_vc[i], i);
    }
    ASSERT(mac->total_frames_sent_in_current_window == 0, "MAC total frames sent in current window should be 0, was: %d", mac->total_frames_sent_in_current_window);
    ASSERT(mac->peer_window_length == 400, "MAC peer window length should be 400, was: %d", mac->peer_window_length);
    // Using the minimum value for T0.
    mac_update_belief(mac, 2600, 2005, 500, 0);
    // Check that values are set.
    ASSERT(mac->T0 == 2680-get_cycle(mac), "MAC T0 should be %d, was: %d", 2680-get_cycle(mac) , mac->T0);
    ASSERT(mac->last_belief_update == 2600, "MAC last belief update should be 2600, was: %d", mac->last_belief_update);
    ASSERT(mac->peer_window_length == 500, "MAC peer window length should be 500, was: %d", mac->peer_window_length);

    // Case where peer window length is greater than max window length.
    mac_update_belief(mac, 2900, 2305, 2000, 0);
    ASSERT(mac->peer_window_length == mac->config->maximum_window_length_ticks, "MAC peer window length should be %d, was: %d", mac->config->maximum_window_length_ticks, mac->peer_window_length);
    ASSERT(mac->T0 == 2980-get_cycle(mac), "MAC T0 should be %d, was: %d", 2980-get_cycle(mac) , mac->T0);
    ASSERT(mac->last_belief_update == 2900, "MAC last belief update should be 2900, was: %d", mac->last_belief_update);

    // Case where peer window length is less than min window length.
    mac_update_belief(mac, 3200, 2305, 100, 0);
    ASSERT(mac->peer_window_length == mac->config->minimum_window_length_ticks, "MAC peer window length should be %d, was: %d", mac->config->minimum_window_length_ticks, mac->peer_window_length);
    ASSERT(mac->T0 == 3280-get_cycle(mac), "MAC T0 should be %d, was: %d", 3280-get_cycle(mac) , mac->T0);
    ASSERT(mac->last_belief_update == 3200, "MAC last belief update should be 3200, was: %d", mac->last_belief_update);

    // Case where peer mac remaining is greater than peer mac length.
    mac_update_belief(mac, 3500, 2905, 700, 800);
    ASSERT(mac->peer_window_length == 700, "MAC peer window length should be 700, was: %d", mac->peer_window_length);
    ASSERT(mac->T0 == 3685-get_cycle(mac), "MAC T0 should be %d, was: %d", 3685-get_cycle(mac) , mac->T0);
    ASSERT(mac->last_belief_update == 3500, "MAC last belief update should be 3500, was: %d", mac->last_belief_update);

    // Now and recieve time are on different sides of the tick modulo.
    mac_update_belief(mac, 1500, MOD_TIME_TICKS-100, 250, 0);
    ASSERT(mac->peer_window_length == 250, "MAC peer window length should be 250, was: %d", mac->peer_window_length);
    ASSERT(mac->T0 == 1580-get_cycle(mac), "MAC T0 should be %d, was: %d", 1580-get_cycle(mac) , mac->T0);
    ASSERT(mac->last_belief_update == 1500, "MAC last belief update should be 100, was: %d", mac->last_belief_update);

    // Destroy the handle.
    sky_destroy(handle);
    // Free the config.
    SKY_FREE(config);
    // Destroy the mac.
    sky_mac_destroy(mac);
}
// Test carrier sense.
TEST(mac_carrier_sense){
    // Default config.
    SkyConfig* config = malloc(sizeof(SkyConfig));
    default_config(config);
    // Create SkyHandle struct.
    SkyHandle handle = sky_create(config);
    // Create SkyMAC struct.
    SkyMAC* mac = sky_mac_create(&handle->conf->mac);

    // Tests:
    ASSERT(mac->config->carrier_sense_ticks == 200, "MAC carrier sense ticks should be 200, was: %d", mac->config->carrier_sense_ticks);

    // Case where time to window opening is greater than carrier sense ticks, T0 should not change.
    sky_mac_carrier_sensed(mac, mac->my_window_length+mac->config->gap_constant_ticks+mac->peer_window_length+mac->config->tail_constant_ticks-201);
    ASSERT(mac->T0 == 0, "MAC T0 should be 0, was: %d", mac->T0);

    // Case where time to window opening is less than carrier sense ticks, T0 should change.
    sky_mac_carrier_sensed(mac, 1100);
    ASSERT(mac->T0 == (1100 - get_cycle(mac)) + 200, "MAC T0 should be %d, was: %d", (1100 - get_cycle(mac)) + 200, mac->T0);

    // Destroy the handle.
    sky_destroy(handle);
    // Free the config.
    SKY_FREE(config);
    // Destroy the mac.
    sky_mac_destroy(mac);
}

// Test if idle frame is needed.
TEST(mac_is_idle_frame_needed){
    // Default config and config where idle frames are not set up.
    SkyConfig* config = malloc(sizeof(SkyConfig));
    SkyConfig* config2 = malloc(sizeof(SkyConfig));
    default_config(config);
    default_config(config2);
    config->mac.idle_frames_per_window = 3;
    config2->mac.idle_frames_per_window = 0;
    // Create SkyHandle struct.
    SkyHandle handle = sky_create(config);
    // Create SkyMAC struct.
    SkyMAC* mac = sky_mac_create(&handle->conf->mac);
    mac->last_belief_update = 1000;

    // Tests:

    // Case where timed out (now - last belief update > idle timeout ticks) and idle frames are set up:
    ASSERT(mac->config->idle_timeout_ticks == 30000, "MAC idle timeout ticks should be 30000, was: %d", mac->config->idle_timeout_ticks);
    ASSERT(mac_idle_frame_needed(mac, 50000) == false, "Idle frame should not be needed, was: %d", mac_idle_frame_needed(mac, 50000));
    
    // Idle frame needed:
    ASSERT(mac->config->idle_frames_per_window == 3, "MAC idle frames per window should be 3, was: %d", mac->config->idle_frames_per_window);
    ASSERT(mac_idle_frame_needed(mac, 20000) == true, "Idle frame should be needed, was: %d", mac_idle_frame_needed(mac, 20000));
    
    // Idle frame not needed:
    mac->total_frames_sent_in_current_window = 3;
    ASSERT(mac_idle_frame_needed(mac, 20000) == false, "Idle frame should not be needed, was: %d", mac_idle_frame_needed(mac, 20000));
    
    // Case where idle frames are not set up:
    // Set config to config2.
    mac->config = &config2->mac;
    ASSERT(mac->config->idle_timeout_ticks == 30000, "MAC idle timeout ticks should be 0, was: %d", mac->config->idle_timeout_ticks);
    ASSERT(mac_idle_frame_needed(mac, 0) == false, "Idle frame should not be needed, was: %d", mac_idle_frame_needed(mac, 0));

    // Destroy the handle.
    sky_destroy(handle);
    // Free the config.
    SKY_FREE(config);
    SKY_FREE(config2);
    // Destroy the mac.
    sky_mac_destroy(mac);
}

// TODO: Test adding frame fields to mac frame in frame_tests.c.