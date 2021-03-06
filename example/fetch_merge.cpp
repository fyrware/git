#include <git2.h>

#include <iostream>

namespace fetch_merge {
    const char* path = "/home/nick/Projects/fyrware/git/delete_this/repository";

    int cred_acquire_cb (git_cred **cred, const char *url, const char *username_from_url, unsigned int allowed_types, void *payload) {
        git_cred_ssh_key_new(cred, username_from_url, nullptr, "/home/nick/.ssh/id_rsa", nullptr);

        return 0;
    }

    int shutdown (git_repository* rep) {
        git_repository_free(rep);
        git_libgit2_shutdown();

        return 0;
    }

    int main () {
        git_libgit2_init();

        int error = 0;

        git_repository* rep = nullptr;
        error = git_repository_open(&rep, path);

        if (error < 0) {
            const git_error *e = giterr_last();
            std::cout << "Error: " << error << " / " << e->klass << " : " << e->message << std::endl;
            return shutdown(rep);
        }

        git_remote* remote = nullptr;
        error = git_remote_lookup(&remote, rep, "origin");

        git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
        fetch_opts.callbacks.credentials = cred_acquire_cb;

        error = git_remote_fetch(remote, nullptr, &fetch_opts, nullptr);

        if (error < 0) {
            const git_error *e = giterr_last();
            std::cout << "Error: " << error << " / " << e->klass << " : " << e->message << std::endl;
            return shutdown(rep);
        }

        git_reference* origin_master = nullptr;
        error = git_branch_lookup(&origin_master, rep, "origin/master", GIT_BRANCH_REMOTE);

        git_reference* local_master = nullptr;
        error = git_branch_lookup(&local_master, rep, "master", GIT_BRANCH_LOCAL);
        error = git_repository_set_head(rep, git_reference_name(local_master));

        // BEGIN MERGE OPERATION

        const git_annotated_commit* their_head[10];
        git_annotated_commit_from_ref((git_annotated_commit **)&their_head[0], rep, origin_master);

        git_merge_options merge_opt = GIT_MERGE_OPTIONS_INIT;
        git_checkout_options checkout_opt = GIT_CHECKOUT_OPTIONS_INIT;
        error = git_merge(rep, their_head, 1, &merge_opt, &checkout_opt);

        if (error < 0) {
            const git_error *e = giterr_last();
            std::cout << "Error: " << error << " / " << e->klass << " : " << e->message << std::endl;

            return shutdown(rep);
        }

        git_index* index = nullptr;
        git_repository_index(&index, rep);

        if (git_index_has_conflicts(index)) {
            git_index_conflict_iterator* conflict_iterator = nullptr;
            git_index_conflict_iterator_new(&conflict_iterator, index);

            const git_index_entry* ancestor_out = nullptr;
            const git_index_entry* our_out = nullptr;
            const git_index_entry* their_out = nullptr;

            while (git_index_conflict_next(&ancestor_out, &our_out, &their_out, conflict_iterator) != GIT_ITEROVER) {
                if (ancestor_out) std::cout<< "ancestor: " << ancestor_out->path <<std::endl;
                if (our_out) std::cout<< "our: " << our_out->path <<std::endl;
                if (their_out) std::cout<< "their: " << their_out->path <<std::endl;
            }

            git_checkout_options opt = GIT_CHECKOUT_OPTIONS_INIT;
            opt.checkout_strategy |= GIT_CHECKOUT_USE_THEIRS;

            git_checkout_index(rep, index, &opt);
            git_index_conflict_iterator_free(conflict_iterator);
        }

        git_commit* their_commit = nullptr;
        git_commit_lookup(&their_commit, rep, git_reference_target(origin_master));

        git_commit* our_commit = nullptr;
        git_commit_lookup(&our_commit, rep, git_reference_target(local_master));

        error = git_index_update_all(index, nullptr, nullptr, nullptr);
        error = git_index_write(index);

        git_oid new_tree_id;
        error = git_index_write_tree(&new_tree_id, index);

        git_tree* new_tree = nullptr;
        error = git_tree_lookup(&new_tree, rep, &new_tree_id);

        git_signature* me = nullptr;
        git_signature_now(&me, "XiaochenFTX", "xiaochenftx@gmail.com");

        git_oid commit_id;
        error = git_commit_create_v(&commit_id, rep, git_reference_name(local_master), me, me, "UTF-8", "pull commit", new_tree, 2, our_commit, their_commit);

        git_repository_state_cleanup(rep);

        return shutdown(rep);
    }
}