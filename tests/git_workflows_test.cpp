#include "git.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>
namespace fs = std::filesystem;
void require(bool b,const char* text) { if (!b) throw std::runtime_error(text); }
template<class F> void rejects(F f) { bool failed=false; try { f(); } catch (const std::exception&) { failed=true; } require(failed,"Expected rejection"); }
void write(const fs::path& p,const std::string& s) { std::ofstream(p,std::ios::binary) << s; }
std::string read(const fs::path& p) { std::ifstream f(p,std::ios::binary); return {std::istreambuf_iterator<char>(f),{}}; }
std::vector<int> changes(const std::string& patch,const std::string& match = {}) {
    std::istringstream in(patch); std::vector<int> out; bool hunk=false; int i=0;
    for (std::string line; std::getline(in,line); ++i) {
        if (line.rfind("@@ ",0)==0) hunk=true;
        else if (hunk && !line.empty() && (line[0]=='+' || line[0]=='-') && (match.empty() || line.substr(1)==match)) out.push_back(i);
    } return out;
}
eg::File file(const eg::Git& git,const std::string& path) { for (auto f:git.load().files) if(f.path==path) return f; throw std::runtime_error("File missing"); }
int main() {
    char pattern[]="/tmp/easy-git-workflows-XXXXXX"; auto* dir=mkdtemp(pattern); if(!dir) return 1; fs::path root(dir), repo=root/"repo";
    try {
        eg::Git::initialize(repo.string(),"main"); eg::Git git(repo.string());
        git.checked({"config","user.name","Workflow Test"}); git.checked({"config","user.email","test@example.invalid"}); git.checked({"config","commit.gpgsign","false"});
        rejects([&]{eg::Git::initialize(repo.string(),"other");});
        write(repo/"file.txt","one\ntwo\nthree\n"); git.stage({"file.txt"}); git.commit("base");
        write(repo/"file.txt","ONE\ntwo\nTHREE\n"); auto f=file(git,"file.txt"); auto patch=git.diff(f,false);
        auto selection=changes(patch,"one"); auto plus=changes(patch,"ONE"); selection.insert(selection.end(),plus.begin(),plus.end());
        git.stage_lines(f,false,patch,selection);
        require(git.checked({"show",":file.txt"})=="ONE\ntwo\nthree\n","Selected replacement staged other lines");
        require(read(repo/"file.txt")=="ONE\ntwo\nTHREE\n","Partial staging changed working file");
        rejects([&]{git.stage_lines(f,false,patch,selection);});
        f=file(git,"file.txt"); patch=git.diff(f,true); git.stage_lines(f,true,patch,changes(patch));
        require(git.checked({"show",":file.txt"})=="one\ntwo\nthree\n","Partial unstage failed");
        std::string many; for(int i=0;i<30;++i) many += "line "+std::to_string(i)+"\n";
        write(repo/"hunks.txt",many); git.stage({"hunks.txt"}); git.commit("hunks");
        auto edited=many; edited.replace(edited.find("line 1\n"),7,"FIRST\n"); edited.replace(edited.find("line 28\n"),8,"LAST\n"); write(repo/"hunks.txt",edited);
        f=file(git,"hunks.txt"); patch=git.diff(f,false); selection=changes(patch,"line 28"); plus=changes(patch,"LAST"); selection.insert(selection.end(),plus.begin(),plus.end());
        git.stage_lines(f,false,patch,selection); auto expected=many; expected.replace(expected.find("line 28\n"),8,"LAST\n");
        require(git.checked({"show",":hunks.txt"})==expected,"Later hunk coordinates were wrong");
        // New/deleted files, unusual names, no final newline, and an unborn branch.
        const std::string name="新 file\n[1].txt"; write(repo/name,"alpha\nbeta"); f=file(git,name); patch=git.diff(f,false);
        git.stage_lines(f,false,patch,changes(patch,"beta")); require(git.checked({"show",":"+name})=="beta","New file/EOF partial stage failed");
        f=file(git,name); patch=git.diff(f,true); git.stage_lines(f,true,patch,changes(patch)); require(git.checked({"ls-files","--stage","--",name}).empty(),"Unstage new file left index entry");
        git.stage({name}); git.commit("new file"); fs::remove(repo/name); f=file(git,name); patch=git.diff(f,false);
        git.stage_lines(f,false,patch,changes(patch,"alpha")); require(git.checked({"show",":"+name})=="beta","Partial deletion removed all content");
        f=file(git,name); patch=git.diff(f,false); git.stage_lines(f,false,patch,changes(patch)); require(git.checked({"ls-files","--stage","--",name}).empty(),"Whole deletion left index entry");
        f=file(git,name); patch=git.diff(f,true); git.stage_lines(f,true,patch,changes(patch)); require(git.checked({"show",":"+name})=="alpha\nbeta","Unstage deletion failed");
        write(repo/"eof.txt","old"); git.stage({"eof.txt"}); git.commit("EOF base");
        write(repo/"eof.txt","new"); f=file(git,"eof.txt"); patch=git.diff(f,false);
        rejects([&]{git.stage_lines(f,false,patch,changes(patch,"new"));});
        require(git.checked({"show",":eof.txt"})=="old","Rejected EOF selection changed the index");
        git.stage_lines(f,false,patch,changes(patch)); require(git.checked({"show",":eof.txt"})=="new","EOF replacement hunk failed");
        fs::path unborn=root/"unborn"; eg::Git::initialize(unborn.string(),"main"); eg::Git fresh(unborn.string());
        write(unborn/"new.txt","first\nsecond\n"); f=file(fresh,"new.txt"); patch=fresh.diff(f,false); fresh.stage_lines(f,false,patch,changes(patch,"first"));
        require(fresh.checked({"show",":new.txt"})=="first\n","Partial stage in unborn branch failed");
        f=file(fresh,"new.txt"); patch=fresh.diff(f,true); fresh.stage_lines(f,true,patch,changes(patch));
        require(fresh.checked({"ls-files"}).empty(),"Partial unstage in unborn branch failed");
        // Discard selected working-tree changes while keeping the index and other hunks.
        write(repo/"discard.txt",many); git.stage({"discard.txt"}); git.commit("discard base");
        auto staged=many; staged.replace(staged.find("line 0\n"),7,"STAGED\n");
        write(repo/"discard.txt",staged); git.stage({"discard.txt"});
        auto working=staged; working.replace(working.find("line 1\n"),7,"FIRST\n"); working.replace(working.find("line 28\n"),8,"LAST\n");
        write(repo/"discard.txt",working); auto tree=git.checked({"write-tree"});
        f=file(git,"discard.txt"); patch=git.diff(f,false); selection=changes(patch,"line 28"); plus=changes(patch,"LAST"); selection.insert(selection.end(),plus.begin(),plus.end());
        git.discard_lines(f,patch,selection);
        auto kept=staged; kept.replace(kept.find("line 1\n"),7,"FIRST\n");
        require(read(repo/"discard.txt")==kept && git.checked({"write-tree"})==tree,"Discard hunk changed staged content or other hunks");
        rejects([&]{git.discard_lines(f,patch,selection);});
        f=file(git,"discard.txt"); patch=git.diff(f,false); selection=changes(patch,"FIRST");
        git.discard_lines(f,patch,selection);
        require(read(repo/"discard.txt")==staged.substr(0,7)+staged.substr(14),"Discard added line changed unrelated content");
        f=file(git,"discard.txt"); patch=git.diff(f,false);
        write(repo/"discard.txt","external edit\n");
        rejects([&]{git.discard_lines(f,patch,changes(patch));});
        require(read(repo/"discard.txt")=="external edit\n" && git.checked({"write-tree"})==tree,"Stale discard damaged file/index");
        write(repo/"discard.txt",staged); fs::remove(repo/"discard.txt");
        f=file(git,"discard.txt"); patch=git.diff(f,false); git.discard_lines(f,patch,changes(patch,"line 28"));
        require(read(repo/"discard.txt")=="line 28\n" && git.checked({"write-tree"})==tree,"Partial restore of deleted file failed");
        const std::string loose="loose 新\n[1].txt"; write(repo/loose,"first\nsecond");
        f=file(git,loose); patch=git.diff(f,false); git.discard_lines(f,patch,changes(patch,"first"));
        require(read(repo/loose)=="second","Discard untracked line failed");
        f=file(git,loose); patch=git.diff(f,false); git.discard_lines(f,patch,changes(patch));
        require(!fs::exists(repo/loose) && git.checked({"write-tree"})==tree,"Discard complete new hunk failed");
        write(repo/"discard.txt",staged); f=file(git,"discard.txt");
        rejects([&]{git.discard_lines(f,git.diff(f,false),{});});
        write(repo/"discard.txt",std::string("a\0b",3)); f=file(git,"discard.txt");
        rejects([&]{git.discard_lines(f,git.diff(f,false),{0});});
        require(read(repo/"discard.txt")==std::string("a\0b",3),"Binary discard modified file");
        fs::remove(repo/"discard.txt"); fs::create_symlink("file.txt",repo/"discard.txt"); f=file(git,"discard.txt");
        rejects([&]{git.discard_lines(f,git.diff(f,false),{0});}); fs::remove(repo/"discard.txt");
        git.checked({"reset","--hard","HEAD"});
        git.checked({"switch","-c","side"}); write(repo/"file.txt","theirs\n"); git.stage({"file.txt"}); git.commit("theirs");
        git.checked({"switch","main"}); write(repo/"file.txt","ours\n"); git.stage({"file.txt"}); git.commit("ours");
        rejects([&]{git.merge("side");}); auto conflict=git.read_conflict(file(git,"file.txt"));
        require(conflict.ours=="ours\n" && conflict.theirs=="theirs\n" && !conflict.base.empty(),"Conflict stages unavailable");
        rejects([&]{git.save_resolution(conflict,conflict.working);});
        write(repo/"file.txt","external edit\n"); rejects([&]{git.save_resolution(conflict,"resolved\n");});
        conflict=git.read_conflict(file(git,"file.txt")); git.save_resolution(conflict,"ours and theirs\n");
        require(git.checked({"ls-files","-u"}).empty() && git.checked({"show",":file.txt"})=="ours and theirs\n","Conflict was not saved and staged");
        git.resolve_operation("merge","continue");
        const std::string binary_base("base\0data",9), binary_ours("ours\0data",9), binary_theirs("theirs\0data",11);
        write(repo/"binary.dat",binary_base); git.stage({"binary.dat"}); git.commit("binary base");
        git.checked({"switch","-c","binary-side"}); write(repo/"binary.dat",binary_theirs); git.stage({"binary.dat"}); git.commit("binary theirs");
        git.checked({"switch","main"}); write(repo/"binary.dat",binary_ours); git.stage({"binary.dat"}); git.commit("binary ours");
        rejects([&]{git.merge("binary-side");}); conflict=git.read_conflict(file(git,"binary.dat"));
        require(conflict.binary && conflict.ours==binary_ours && conflict.theirs==binary_theirs,"Binary conflict versions were corrupted");
        git.save_resolution(conflict,conflict.theirs); git.resolve_operation("merge","continue");
        require(read(repo/"binary.dat")==binary_theirs,"Binary resolution was truncated at NUL");
        git.checked({"switch","-c","delete-side"}); git.checked({"rm","file.txt"}); git.commit("delete file");
        git.checked({"switch","main"}); write(repo/"file.txt","modified instead of deleted\n"); git.stage({"file.txt"}); git.commit("modify file");
        rejects([&]{git.merge("delete-side");}); conflict=git.read_conflict(file(git,"file.txt"));
        require(conflict.has_ours && !conflict.has_theirs,"Modify/delete conflict did not expose deletion");
        git.save_resolution(conflict,"",true); require(!fs::exists(repo/"file.txt") && git.checked({"ls-files","-u"}).empty(),"Accept deletion did not resolve the conflict");
        git.resolve_operation("merge","continue");
        git.checked({"branch","delete-me"}); eg::Ref branch; for(auto r:git.load().refs) if(r.name=="delete-me") branch=r;
        git.delete_branch(branch); require(git.run({"show-ref","--verify","refs/heads/delete-me"}).code!=0,"Branch not deleted");
        for(auto r:git.load().refs) if(r.name=="main") rejects([&]{git.delete_branch(r,true);});
        git.checked({"branch","unmerged","side"}); // Already merged, create a new unmerged tip instead.
        git.checked({"switch","unmerged"}); write(repo/"unique","unique"); git.stage({"unique"}); git.commit("unique"); git.checked({"switch","main"});
        for(auto r:git.load().refs) if(r.name=="unmerged") { rejects([&]{git.delete_branch(r);}); git.delete_branch(r,true); }
        fs::path remote=root/"remote.git"; eg::Git(root.string()).checked({"init","--bare","--initial-branch=main",remote.string()});
        git.checked({"remote","add","origin",remote.string()}); git.checked({"push","-u","origin","main"});
        eg::Git::clone(remote.string(),(root/"cloned repo").string()); require(eg::Git((root/"cloned repo").string()).load().has_head,"Clone did not create a usable repository");
        rejects([&]{eg::Git::clone(remote.string(),repo.string());});
        auto target=git.push_target(); git.force_push(target);
        git.checked({"reset","--hard","HEAD~1"}); target=git.push_target(); git.force_push(target);
        require(eg::Git(remote.string()).checked({"rev-parse","main"})==git.checked({"rev-parse","HEAD"}),"Force push did not update remote");
        // Another clone changes the remote after confirmation: lease must refuse overwriting it.
        auto other=eg::Git((root/"cloned repo").string()); other.checked({"config","user.name","Other"}); other.checked({"config","user.email","other@example.invalid"}); other.checked({"config","commit.gpgsign","false"});
        write(root/"cloned repo/another","another"); other.stage({"another"}); other.commit("remote advanced"); other.checked({"push","origin","main"});
        rejects([&]{git.force_push(target);});
        git.checked({"fetch","origin"}); git.checked({"push","origin","main:temporary"}); git.checked({"fetch","origin"});
        for(auto r:git.load().refs) if(r.full=="refs/remotes/origin/temporary") git.delete_branch(r);
        require(eg::Git(remote.string()).run({"show-ref","--verify","refs/heads/temporary"}).code!=0,"Remote branch not deleted");
        std::cout<<"PASS: partial staging/unstaging, conflict editor, init/clone, branch deletion, force-with-lease\n";
        fs::remove_all(root); return 0;
    } catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\nFixture: "<<root<<'\n'; return 1; }
}
