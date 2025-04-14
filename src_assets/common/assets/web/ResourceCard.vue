<template>
    <div class="card p-2 mt-4">
        <div class="card-body">
            <h2>{{ $t('resource_card.resources') }}</h2>
            <br>
            <p>{{ $t('resource_card.resources_desc') }}</p>
            <div class="card-group p-4 align-items-center">
                <!-- <a class="btn btn-success m-1" href="https://app.lizardbyte.dev" target="_blank">
                    {{ $t('resource_card.lizardbyte_website') }}</a> -->
                <a class="btn btn-secondary m-1" href="https://github.com/ClassicOldSong/Apollo/discussions" target="_blank">
                    <i class="fab fa-fw fa-github"></i> {{ $t('resource_card.github_discussions') }}</a>
                <a class="btn btn-secondary m-1" href="https://github.com/ClassicOldSong/Apollo/wiki" target="_blank">
                    <i class="fab fa-fw fa-github"></i> {{ $t('resource_card.github_wiki') }}</a>
            </div>
        </div>
    </div>
    <!-- Legal -->
    <div class="card p-2 mt-4">
        <div class="card-body">
            <h2>{{ $t('resource_card.legal') }}</h2>
            <br>
            <p>{{ $t('resource_card.legal_desc') }}</p>
            <div class="card-group p-4 align-items-center">
                <a class="btn btn-danger m-1" href="https://github.com/ClassicOldSong/Apollo/blob/master/LICENSE"
                    target="_blank">
                    <i class="fas fa-fw fa-file-alt"></i> {{ $t('resource_card.license') }}</a>
                <a class="btn btn-danger m-1" href="https://github.com/ClassicOldSong/Apollo/blob/master/NOTICE"
                    target="_blank">
                    <i class="fas fa-fw fa-exclamation"></i> {{ $t('resource_card.third_party_notice') }}</a>
            </div>
        </div>
    </div>
</template>
